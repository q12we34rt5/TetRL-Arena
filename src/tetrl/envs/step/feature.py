"""
Feature (observation) plugin for the step-based environment.

Provides :class:`FeaturePlugin` -- the abstract contract every observation
extractor must fulfil -- and :class:`CppFeature`, a concrete implementation
that JIT-compiles user-supplied C++ code via :mod:`~tetrl.dynamic_library`.

Both ``tetris.hpp`` functions and the step-env types (``Info``,
``Action``, ...) are available in user C++ code.
"""

from __future__ import annotations

import ctypes
import os
from abc import ABC, abstractmethod
from typing import Any, Callable, TYPE_CHECKING, Sequence

import numpy as np

from ... import dynamic_library as dl
from ...native_layout import CSRC_DIR, csrc_path
from .native import StepEnvContext, StepInfo

if TYPE_CHECKING:
    import gymnasium

_ENGINE_CPP = "engine/tetris.cpp"
_ENGINE_HPP = "engine/tetris.hpp"
_STEP_HPP = "envs/step/step.hpp"


class FeaturePlugin(ABC):
    """Abstract base class for feature (observation) plugins.

    Subclass this to create custom observation extractors for the
    step-based Tetris RL environment.  The environment calls
    :meth:`reset` once at the start of each episode and :meth:`step`
    after every action.
    """

    @abstractmethod
    def observation_space(self) -> "gymnasium.spaces.Space":
        """Return the gymnasium observation space produced by this plugin."""
        ...

    @abstractmethod
    def reset(self, ctx: StepEnvContext) -> Any:
        """Reset plugin state and return the initial observation.

        Parameters
        ----------
        ctx:
            The environment context after reset.

        Returns
        -------
        Any
            The initial observation for the episode.
        """
        ...

    @abstractmethod
    def step(self, ctx: StepEnvContext, step_info: StepInfo) -> Any:
        """Compute the observation after a step.

        Parameters
        ----------
        ctx:
            The environment context **after** the step.
        step_info:
            A :class:`StepInfo` containing ``action_id``,
            ``action_success``, and ``forced_hard_drop``. Plugins may
            use ``forced_hard_drop`` to distinguish a normal
            non-locking action from a lifetime-expiry lock.

        Returns
        -------
        Any
            The observation for the current step.
        """
        ...


class CppFeature(FeaturePlugin):
    """Feature plugin backed by JIT-compiled C++ code.

    ``tetris.cpp`` and ``step.hpp`` are compiled together with the user
    source, so all engine types/functions (``State``, ``removeCurrentPiece``,
    ``placeCurrentPiece``, ...) **and** step-env types (``Info``,
    ``Action``, ...) are available without additional includes.

    Required C++ functions
    ----------------------
    The source must define the following ``API``-prefixed functions::

        API int  feature_context_size()                        // bytes; 0 = stateless
        API int  feature_size()
        API void feature_reset(Context* ctx, void* plugin_ctx)
        API void feature_step(Context* ctx, Info* info,
                              void* plugin_ctx, float* out)

    ``feature_context_size`` returns the byte size of the user-defined
    feature plugin context struct (return ``0`` if no per-instance state
    is needed). Python allocates a buffer of that size for each
    :class:`CppFeature` instance and passes it as ``plugin_ctx`` on every
    call.

    ``feature_reset`` is called once per episode to initialise plugin
    state.  ``feature_step`` is called after every action to compute the
    observation vector.  The environment calls ``feature_step`` with a
    zeroed ``Info`` after ``feature_reset`` to obtain the initial
    observation.  The output buffer size is defined by
    ``feature_size()`` and guaranteed by Python.

    Parameters
    ----------
    source:
        C++ source code.
    observation_space:
        A pre-built gymnasium observation space.  When provided,
        ``observation_low`` / ``observation_high`` / ``observation_dtype``
        are ignored and :meth:`observation_space` returns this directly.
    observation_low:
        Lower bound for the default ``gymnasium.spaces.Box`` (ignored
        when *observation_space* is given).
    observation_high:
        Upper bound for the default ``gymnasium.spaces.Box``.
    observation_dtype:
        Data type for the default ``gymnasium.spaces.Box``.
    unpack:
        Optional callable ``(np.ndarray) -> Any`` applied to the raw
        flat buffer before returning from :meth:`reset` / :meth:`step`.
        Use this to reshape, split, or wrap the flat ``float32`` output
        into dicts, tuples, or other structures expected by your agent.
    extra_compile_flags:
        Additional flags passed to the C++ compiler.
    watch_files:
        Extra header / source files whose content should invalidate
        the compilation cache when changed.

    Examples
    --------
    Stateless (simplest):

    >>> feature = CppFeature(r'''
    ...     using namespace ops;
    ...     API int  feature_context_size() { return 0; }
    ...     API int  feature_size() { return 200; }
    ...     API void feature_reset(Context*, void*) {}
    ...     API void feature_step(Context* ctx, Info* info,
    ...                           void*, float* out) {
    ...         State* s = &ctx->state;
    ...         for (int r = 0; r < 20; ++r)
    ...             for (int c = 0; c < 10; ++c)
    ...                 out[r * 10 + c] = (getCell(s->board,
    ...                     BOARD_LEFT + c, BOARD_TOP + r)
    ...                     != Cell::EMPTY) ? 1.0f : 0.0f;
    ...     }
    ... ''')

    Stateful with custom observation space + unpack:

    >>> feature = CppFeature(
    ...     r'''
    ...     using namespace ops;
    ...     struct FeatureContext { int8_t prev_x, prev_y; };
    ...     API int  feature_context_size() { return sizeof(FeatureContext); }
    ...     API int  feature_size() { return 202; }
    ...     API void feature_reset(Context* ctx, void* plugin_ctx) {
    ...         State* s = &ctx->state;
    ...         auto* c = static_cast<FeatureContext*>(plugin_ctx);
    ...         c->prev_x = s->x;
    ...         c->prev_y = s->y;
    ...     }
    ...     API void feature_step(Context* ctx, Info*,
    ...                           void* plugin_ctx, float* out) {
    ...         State* s = &ctx->state;
    ...         auto* c = static_cast<FeatureContext*>(plugin_ctx);
    ...         // ... fill board into out[0..199]
    ...         out[200] = static_cast<float>(s->x - c->prev_x);
    ...         out[201] = static_cast<float>(s->y - c->prev_y);
    ...         c->prev_x = s->x;
    ...         c->prev_y = s->y;
    ...     }
    ...     ''',
    ...     observation_space=gymnasium.spaces.Dict({
    ...         "board": gymnasium.spaces.Box(0, 1, (20, 10)),
    ...         "delta_pos": gymnasium.spaces.Box(-np.inf, np.inf, (2,)),
    ...     }),
    ...     unpack=lambda buf: {
    ...         "board": buf[:200].reshape(20, 10),
    ...         "delta_pos": buf[200:202],
    ...     },
    ... )
    """

    def __init__(
        self,
        source: str,
        *,
        observation_space: "gymnasium.spaces.Space | None" = None,
        observation_low: float = -np.inf,
        observation_high: float = np.inf,
        observation_dtype: np.dtype = np.float32,
        unpack: Callable[[np.ndarray], Any] | None = None,
        extra_compile_flags: Sequence[str] | None = None,
        watch_files: Sequence[str | os.PathLike] | None = None,
    ) -> None:
        self._custom_obs_space = observation_space
        self._obs_low = observation_low
        self._obs_high = observation_high
        self._obs_dtype = np.dtype(observation_dtype)
        self._unpack = unpack

        # Include engine source + step header so the user has everything.
        full_source = f'#include "{_ENGINE_CPP}"\n#include "{_STEP_HPP}"\n\nusing namespace tetrl;\nusing namespace tetrl::envs::step;\n\n{source}\n'

        compile_flags = [
            f"-I{CSRC_DIR}",
            "-std=c++17",
            "-O3",
        ]
        if extra_compile_flags:
            compile_flags.extend(extra_compile_flags)

        all_watch = [
            csrc_path(_ENGINE_HPP),
            csrc_path(_ENGINE_CPP),
            csrc_path(_STEP_HPP),
        ]
        all_watch.extend(watch_files or [])

        self._lib = dl.DynamicLibrary(extra_compile_flags=compile_flags)
        self._lib.compile_string(
            full_source,
            watch_files=all_watch,
            functions={
                "feature_context_size": {"argtypes": [], "restype": dl.int32},
                "feature_size": {"argtypes": [], "restype": dl.int32},
                "feature_reset": {"argtypes": [dl.void_p, dl.void_p], "restype": dl.void},
                "feature_step": {"argtypes": [dl.void_p, dl.void_p, dl.void_p, dl.void_p], "restype": dl.void},
            },
        )

        # Allocate per-instance context buffer.
        self._ctx_size: int = int(self._lib.feature_context_size())
        self._ctx_buf = ctypes.create_string_buffer(self._ctx_size) if self._ctx_size > 0 else None
        self._ctx_ptr: int = ctypes.addressof(self._ctx_buf) if self._ctx_buf is not None else 0

        # Query feature dimensionality and pre-allocate output buffer.
        self._size: int = int(self._lib.feature_size())
        self._buf = np.empty(self._size, dtype=np.float32)

    def observation_space(self) -> "gymnasium.spaces.Space":
        if self._custom_obs_space is not None:
            return self._custom_obs_space

        import gymnasium

        return gymnasium.spaces.Box(
            low=self._obs_low,
            high=self._obs_high,
            shape=(self._size,),
            dtype=self._obs_dtype,
        )

    def reset(self, ctx: StepEnvContext) -> Any:
        addr = ctypes.addressof(ctx)
        self._lib.feature_reset(addr, self._ctx_ptr)
        # Produce the initial observation with a zeroed StepInfo.
        dummy = StepInfo()
        self._lib.feature_step(
            addr,
            ctypes.addressof(dummy),
            self._ctx_ptr,
            self._buf.ctypes.data,
        )
        out = self._buf.copy()
        return self._unpack(out) if self._unpack is not None else out

    def step(self, ctx: StepEnvContext, step_info: StepInfo) -> Any:
        self._lib.feature_step(
            ctypes.addressof(ctx),
            ctypes.addressof(step_info),
            self._ctx_ptr,
            self._buf.ctypes.data,
        )
        out = self._buf.copy()
        return self._unpack(out) if self._unpack is not None else out

    @property
    def size(self) -> int:
        """Number of elements in the feature vector."""
        return self._size

    @property
    def context_size(self) -> int:
        """Byte size of the C++ feature plugin context (0 = stateless)."""
        return self._ctx_size

    def close(self) -> None:
        """Release the compiled shared library."""
        self._lib.close()

    def __repr__(self) -> str:
        return f"CppFeature(size={self._size}, context_size={self._ctx_size})"
