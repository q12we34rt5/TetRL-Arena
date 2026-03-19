"""
Python/native bridge for ``step.hpp``.

Responsibility
--------------
This module is the **sole** communication layer between Python and the C++
Tetris engine for the step-based environment.  It:

* JIT-compiles ``tetris.cpp`` and its inline wrappers via :class:`DynamicLibrary`.
* Mirrors every relevant C++ struct as a ``ctypes.Structure`` so Python can
  allocate and pass them by pointer with zero copies.
* Exposes a thin, typed Python API (``env_reset``, ``env_step``, ...) that
  gymnasium environment code calls without knowing anything about ctypes.
"""

from __future__ import annotations

import ctypes
import enum

from ... import dynamic_library as dl
from ...native_layout import CSRC_DIR, csrc_path
from ...engine.state import State

_ENGINE_CPP = "engine/tetris.cpp"
_ENGINE_HPP = "engine/tetris.hpp"
_STEP_HPP = "envs/step/step.hpp"


class Action(enum.IntEnum):
    MOVE_LEFT = 0
    MOVE_RIGHT = 1
    SOFT_DROP = 2
    HARD_DROP = 3
    ROTATE_CW = 4
    ROTATE_CCW = 5
    ROTATE_180 = 6
    HOLD = 7
    MOVE_LEFT_TO_WALL = 8
    MOVE_RIGHT_TO_WALL = 9
    SOFT_DROP_TO_FLOOR = 10
    NOOP = 11


N_ACTIONS: int = 12  # == Action::SIZE  (sentinel, not a valid action)


class StepInfo(ctypes.Structure):
    """Mirror of ``tetrl::envs::step::Info`` in ``step.hpp``."""

    _fields_ = [
        ("action_id", ctypes.c_uint8),  # which action was passed
        ("action_success", ctypes.c_uint8),  # 1 if the selected action returned success
        ("forced_hard_drop", ctypes.c_uint8),  # 1 if lifetime forced a hard drop
    ]

    def __repr__(self) -> str:
        return f"StepInfo(action_id={self.action_id}, action_success={bool(self.action_success)}, forced_hard_drop={bool(self.forced_hard_drop)})"


class StepEnvConfig(ctypes.Structure):
    """Mirror of ``tetrl::envs::step::Config`` in ``step.hpp``.

    Parameters
    ----------
    block_life:
        Number of steps before the engine forces a hard-drop (default 20).
    auto_drop:
        If non-zero, a ``softDrop`` is simulated every step (gravity).
    """

    _fields_ = [
        ("block_life", ctypes.c_int32),
        ("auto_drop", ctypes.c_uint8),
    ]

    def __init__(self, block_life: int = 20, auto_drop: bool = True) -> None:
        super().__init__(block_life=block_life, auto_drop=int(auto_drop))

    def __repr__(self) -> str:
        return f"StepEnvConfig(block_life={self.block_life}, auto_drop={bool(self.auto_drop)})"


class StepEnvContext(ctypes.Structure):
    """Mirror of ``tetrl::envs::step::Context`` in ``step.hpp``."""

    _fields_ = [
        ("state", State),
        ("lifetime", ctypes.c_int32),
        ("config", StepEnvConfig),
    ]

    def __init__(
        self,
        *,
        state: State = State(),
        lifetime: int = 0,
        config: StepEnvConfig = StepEnvConfig(),
    ) -> None:
        super().__init__(state=state, lifetime=lifetime, config=config)


_WRAPPER_SOURCE = (
    f'#include "{_ENGINE_CPP}"\n'
    f'#include "{_STEP_HPP}"\n\n'
    + r"""
using namespace tetrl::envs::step;

API void api_envSetConfig(Context* ctx, Config* config) {
    setConfig(ctx, *config);
}

API void api_envSetSeed(Context* ctx, std::uint32_t seed, std::uint32_t garbage_seed) {
    setSeed(ctx, seed, garbage_seed);
}

API void api_envReset(Context* ctx) {
    reset(ctx);
}

// Use output pointer to avoid struct-return ABI differences.
API void api_envStep(Context* ctx, std::uint8_t action, Info* out) {
    *out = step(ctx, static_cast<Action>(action));
}
"""
)

_lib = dl.DynamicLibrary(
    extra_compile_flags=[
        f"-I{CSRC_DIR}",
        "-std=c++17",
        "-O3",
    ]
)

_lib.compile_string(
    _WRAPPER_SOURCE,
    watch_files=[
        csrc_path(_ENGINE_HPP),
        csrc_path(_ENGINE_CPP),
        csrc_path(_STEP_HPP),
    ],
    functions={
        # All struct pointers are passed as void* (c_void_p); we obtain the
        # actual address via ctypes.addressof() in the public helpers below.
        "api_envSetConfig": {"argtypes": [dl.void_p, dl.void_p], "restype": dl.void},
        "api_envSetSeed": {"argtypes": [dl.void_p, dl.uint32, dl.uint32], "restype": dl.void},
        "api_envReset": {"argtypes": [dl.void_p], "restype": dl.void},
        "api_envStep": {"argtypes": [dl.void_p, dl.uint8, dl.void_p], "restype": dl.void},
    },
)


def env_set_config(ctx: StepEnvContext, config: StepEnvConfig) -> None:
    """Copy *config* into *ctx* (wrapper for ``setConfig``)."""
    _lib.api_envSetConfig(ctypes.addressof(ctx), ctypes.addressof(config))


def env_set_seed(ctx: StepEnvContext, seed: int, garbage_seed: int) -> None:
    """Seed both the piece-bag RNG and the garbage RNG.

    .. warning::

       The engine uses **xorshift32** which has a fixed point at 0.
       Callers must ensure neither *seed* nor *garbage_seed* is 0.
    """
    _lib.api_envSetSeed(ctypes.addressof(ctx), seed, garbage_seed)


def env_reset(ctx: StepEnvContext) -> None:
    """Reset *ctx* to the initial state (calls ``reset`` in C++)."""
    _lib.api_envReset(ctypes.addressof(ctx))


def env_step(ctx: StepEnvContext, action: Action | int) -> StepInfo:
    """Execute one step and return the result.

    Parameters
    ----------
    ctx:
        The environment context to mutate.
    action:
        An :class:`Action` value (or its ``int`` equivalent).

    Returns
    -------
    StepInfo
        Contains ``action_id`` and ``action_success``.
    """
    info = StepInfo()
    _lib.api_envStep(
        ctypes.addressof(ctx),
        int(action),
        ctypes.addressof(info),
    )
    return info
