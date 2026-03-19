"""
JIT-compiles C/C++ source into a shared library and exposes the exported
symbols as Python callables with convenient type conversions.
"""

from __future__ import annotations

import getpass
import hashlib
import os
import platform
import shutil
import subprocess
import tempfile
import textwrap
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Sequence, Tuple

import ctypes

from ._types import void

__all__ = ["DynamicLibrary", "CompileError", "FunctionWrapper"]

_SYSTEM = platform.system()
_IS_WINDOWS = _SYSTEM == "Windows"
_LIB_EXT = {"Windows": ".dll", "Darwin": ".dylib"}.get(_SYSTEM, ".so")


class CompileError(RuntimeError):
    """Raised when C/C++ compilation fails."""

    def __init__(self, command: Sequence[str], stdout: bytes, stderr: bytes) -> None:
        super().__init__(
            "Compilation failed. Command: {}\nstdout:\n{}\nstderr:\n{}".format(
                " ".join(command),
                stdout.decode(errors="replace"),
                stderr.decode(errors="replace"),
            )
        )
        self.command = tuple(command)
        self.stdout = stdout
        self.stderr = stderr


class FunctionWrapper:
    """
    Thin callable that wraps a *ctypes* function pointer and applies the
    converters defined in ``_types.py`` to every positional argument.

    Examples
    --------
    >>> add = lib.add      # -> FunctionWrapper
    >>> add(1, 2)
    3
    """

    def __init__(
        self,
        name: str,
        arg_converters: Sequence,
        restype,
        cfunc: "ctypes._CFuncPtr",
    ):
        self.__name__ = name
        self.__qualname__ = name
        self._arg_conv = arg_converters
        self._cfunc = cfunc
        self.address: Optional[int] = None  # filled later
        self._cfunc.restype = restype.type_mapping if restype else None
        if arg_converters:
            self._cfunc.argtypes = [a.type_mapping for a in arg_converters]

    def __call__(self, *args):
        converted = (conv(arg) if conv is not None else arg for conv, arg in zip(self._arg_conv, args))
        return self._cfunc(*converted)

    def __repr__(self) -> str:
        arg_sig = f"({self._arg_conv[0]})" if len(self._arg_conv) == 1 else tuple(self._arg_conv)
        ret = void if self._cfunc.restype is None else self._cfunc.restype
        return f"{ret} {self.__name__}{arg_sig}"


class DynamicLibrary:
    """
    JIT-compiles C/C++ source into a shared library and exposes selected
    functions as Python callables.

    Parameters
    ----------
    cc:
        Path to C/C++ compiler or command list (default ``"auto"``).
        * On Linux / macOS, ``"auto"`` tries ``g++``, then ``clang++``.
        * On Windows, ``"auto"`` tries ``cl.exe``, then ``g++`` (MinGW).
    cache_dir:
        Directory used to store compiled artefacts keyed by SHA-256 of
        (source + flags).  If ``None``, uses platform temp dir.

    Examples
    --------
    >>> with DynamicLibrary() as lib:
    ...     lib.compile_string(
    ...         "API int add(int a,int b) { return a + b; }",
    ...         functions={
    ...             "add": {"argtypes": [int32, int32], "restype": int32}
    ...         })
    ...     lib.add(2, 3)
    5
    """

    def __init__(
        self,
        cc: str | Sequence[str] = "auto",
        *,
        cache_dir: str | os.PathLike | None = None,
        extra_compile_flags: Optional[Sequence[str]] = None,
        watch_files: Optional[Sequence[str | os.PathLike]] = None,
    ) -> None:
        self._compiler_cmd: Tuple[str, ...] = self._resolve_compiler(cc)
        self._extra_flags = tuple(extra_compile_flags or ())
        self._watch_files: Tuple[Path, ...] = tuple(Path(f).expanduser().resolve() for f in (watch_files or ()))
        if cache_dir is None:
            self._cache_dir = Path(tempfile.gettempdir()) / f"dynlib_cache_{getpass.getuser()}"
        else:
            self._cache_dir = Path(cache_dir)
        self._cache_dir.mkdir(parents=True, exist_ok=True)

        self._tmp_dir_ctx: Optional[tempfile.TemporaryDirectory[str]] = None
        self._tmp_dir_path: Optional[Path] = None
        self._lib_handle: Optional["ctypes.CDLL"] = None
        self._exported: List[str] = []

        # For POSIX `dlclose`
        if not _IS_WINDOWS:
            # ctypes.CDLL(None) == dlopen(NULL, ...) -> global symbol table
            # This reliably exposes dlclose on both Linux and macOS.
            _libc = ctypes.CDLL(None)
            self._dlclose = _libc.dlclose  # type: ignore[attr-defined]
            self._dlclose.argtypes = [ctypes.c_void_p]

    def __enter__(self) -> "DynamicLibrary":
        return self

    def __exit__(self, exc_type, exc, tb) -> bool:
        self.close()
        # propagate exception (if any)
        return False

    def compile_string(
        self,
        source: str,
        *,
        functions: Dict[str, Dict] | None = None,
        prefix: str | None = None,
        watch_files: Optional[Sequence[str | os.PathLike]] = None,
    ) -> None:
        """
        Compile a C/C++ *string* and load the resulting library.

        Parameters
        ----------
        source:
            C/C++ source (without export macros).
        functions:
            ``{"name": {"argtypes": [...], "restype": <type>}, ...}``
            Mapping of function names to metadata dicts with keys
            ``"argtypes"`` (list of types) and ``"restype"`` (return type).
        prefix:
            Extra ``#define`` or ``#include`` before the user source.
            If omitted, uses a default macro that expands ``API``.
        watch_files:
            Extra files whose contents are mixed into the cache key.
            Use this to list every header / ``.cpp`` that the source
            ``#include``s so that edits to those files bust the cache.
            Combined with the instance-level ``watch_files`` passed to
            ``__init__``.
        """
        prefix = prefix or textwrap.dedent(
            r"""
            #ifdef _WIN32
            #   define API extern "C" __declspec(dllexport)
            #else
            #   define API extern "C"
            #endif
            """
        )
        exports = list((functions or {}).keys())
        full_source = prefix + "\n" + source + "\n" + _create_extractors(exports)
        extra = tuple(Path(f).expanduser().resolve() for f in (watch_files or ()))
        self._build_and_load(full_source, functions or {}, extra_watch=extra)

    def compile_file(
        self,
        filepath: str | os.PathLike,
        *,
        functions: Dict[str, Dict] | None = None,
        watch_files: Optional[Sequence[str | os.PathLike]] = None,
    ) -> None:
        """Compile an existing C/C++ file.

        Parameters
        ----------
        filepath:
            Path to the C/C++ source file.
        functions:
            Same as in :meth:`compile_string`.
        watch_files:
            Same as in :meth:`compile_string`.  The compiled file itself is
            always included in the hash automatically.
        """
        path = Path(filepath).expanduser().resolve()
        with path.open("r", encoding="utf-8") as fp:
            source = fp.read()
        extra = tuple(Path(f).expanduser().resolve() for f in (watch_files or ()))
        self._build_and_load(source, functions or {}, extra_watch=extra)

    def close(self) -> None:
        """Unload the shared library and clean up temp files."""
        # remove python attributes
        for name in self._exported:
            try:
                delattr(self, name)
            except AttributeError:
                pass
        self._exported.clear()

        # unload CDLL
        if self._lib_handle is not None:
            if _IS_WINDOWS:
                ctypes.windll.kernel32.FreeLibrary(self._lib_handle._handle)  # type: ignore[attr-defined]
            else:
                self._dlclose(self._lib_handle._handle)  # type: ignore[arg-type]
            self._lib_handle = None

        # dispose temp dir (if any)
        if self._tmp_dir_ctx is not None:
            self._tmp_dir_ctx.cleanup()
            self._tmp_dir_ctx = None
            self._tmp_dir_path = None

    def _build_and_load(
        self,
        source: str,
        functions: Dict[str, Dict[str, object]],
        extra_watch: Tuple[Path, ...] = (),
    ) -> None:
        # 1) check cache
        h = hashlib.sha256()
        h.update(source.encode())
        h.update(b"|cmd=")
        h.update(" ".join(self._compiler_cmd).encode())
        h.update(" ".join(self._extra_flags).encode())
        # Hash all watched files (instance-level + call-level, deduplicated)
        seen: set = set()
        for path in (*self._watch_files, *extra_watch):
            if path in seen:
                continue
            seen.add(path)
            h.update(b"|file=")
            h.update(str(path).encode())
            try:
                h.update(path.read_bytes())
            except OSError:
                pass  # missing files will cause a compile error anyway
        digest = h.hexdigest()
        cached_lib = self._cache_dir / f"lib_{digest}{_LIB_EXT}"
        if not cached_lib.exists():
            # 2) Compile into temp dir
            self._tmp_dir_ctx = tempfile.TemporaryDirectory()
            self._tmp_dir_path = Path(self._tmp_dir_ctx.name)
            src_path = self._tmp_dir_path / "lib.cpp"
            src_path.write_text(source, encoding="utf-8")

            self._compile(src_path, cached_lib)
        # 3) Load
        self._lib_handle = ctypes.CDLL(str(cached_lib))
        self._bind_functions(functions)

    def _compile(self, src_path: Path, output_path: Path) -> None:
        cmd = list(self._compiler_cmd)

        # Windows/MSC uses different flags
        if _IS_WINDOWS and shutil.which("cl.exe") and "cl.exe" in cmd[0].lower():
            # cl: /LD -> DLL, /Fe:<out>
            cmd += ["/LD", "/O2", str(src_path), f"/Fe:{output_path}"]
            cmd.extend(self._extra_flags)
        else:
            # GCC/Clang
            cmd += [str(src_path), "-shared", "-o", str(output_path), "-O3", "-g"]
            if not _IS_WINDOWS:
                cmd.append("-fPIC")
            cmd.extend(self._extra_flags)

        try:
            result = subprocess.run(
                cmd,
                check=True,
                capture_output=True,
                text=False,
            )
        except subprocess.CalledProcessError as exc:
            raise CompileError(exc.cmd, exc.stdout, exc.stderr) from None

        # Optionally keep compile output for debugging:
        if result.stdout or result.stderr:
            (output_path.parent / "compile.log").write_bytes(result.stdout + b"\n" + result.stderr)

    def _bind_functions(self, functions: Dict[str, Dict[str, object]]) -> None:
        for name, meta in functions.items():
            argtypes: List = list(meta.get("argtypes", []))
            restype = meta.get("restype", void)

            # Obtain ctypes function pointer from lib
            cfunc = getattr(self._lib_handle, name)
            wrapper = FunctionWrapper(name, argtypes, restype, cfunc)
            setattr(self, name, wrapper)
            self._exported.append(name)

            # Get raw address via extractor and store to wrapper.address
            extractor_name = f"__get_library_function_pointer_{name}"
            extractor = getattr(self._lib_handle, extractor_name, None)
            if extractor is not None:
                extractor.restype = ctypes.c_void_p
                wrapper.address = extractor()

    @staticmethod
    def _resolve_compiler(cc: str | Sequence[str] = "auto") -> Tuple[str, ...]:
        if cc != "auto":
            if isinstance(cc, str):
                return (cc,)
            return tuple(str(part) for part in cc)

        candidates: Iterable[Tuple[str, ...]]
        if _IS_WINDOWS:
            candidates = (("cl.exe",), ("g++",))
        else:
            candidates = (("g++",), ("clang++",))

        for cand in candidates:
            if shutil.which(cand[0]):
                return cand
        raise FileNotFoundError("No suitable C/C++ compiler found on PATH.")


# generate extractor functions
_EXTRACTOR_FMT = (
    r"void* __get_library_function_pointer_{0}() {{ "
    r"return reinterpret_cast<void*>({0}); }}"
)
_EXTRACTOR_WIN = 'extern "C" __declspec(dllexport) '
_EXTRACTOR_POSIX = 'extern "C" '


def _create_extractors(funcs: Iterable[str]) -> str:
    win = "\n".join(_EXTRACTOR_WIN + _EXTRACTOR_FMT.format(f) for f in funcs)
    posix = "\n".join(_EXTRACTOR_POSIX + _EXTRACTOR_FMT.format(f) for f in funcs)
    return textwrap.dedent(
        f"""
        #ifdef _WIN32
        {win}
        #else
        {posix}
        #endif
        """
    )
