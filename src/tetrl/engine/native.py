"""
Python/native bridge for the base Tetris engine (``tetris.hpp`` / ``tetris.cpp``).

Responsibility
--------------
This module is the **single compilation unit** for the core Tetris engine.
It:

* JIT-compiles ``tetris.cpp`` via :class:`DynamicLibrary` and exports the
  public symbols needed by higher-level code.
* Provides thin, typed Python wrappers so that callers can interact with
  the engine directly without touching ctypes.
"""

from __future__ import annotations

import ctypes

from .. import dynamic_library as dl
from ..native_layout import CSRC_DIR, csrc_path
from .state import State

_ENGINE_CPP = "engine/tetris.cpp"
_ENGINE_HPP = "engine/tetris.hpp"

# All functions in tetris.hpp that return ``bool`` are wrapped to return
# ``uint8_t`` to avoid C++ ABI ambiguity over bool size.
# ``toString`` receives a ``uint64_t`` size so we can use a fixed-width type
# instead of the platform-dependent ``size_t``.

_WRAPPER_SOURCE = (
    f'#include "{_ENGINE_CPP}"\n\n'
    + r"""
using namespace tetrl;

API void     api_setSeed               (State* s, std::uint32_t seed, std::uint32_t garbage_seed) { setSeed(s, seed, garbage_seed); }
API void     api_reset                 (State* s) { reset(s); }

API uint8_t  api_moveLeft              (State* s) { return moveLeft(s); }
API uint8_t  api_moveRight             (State* s) { return moveRight(s); }
API uint8_t  api_moveLeftToWall        (State* s) { return moveLeftToWall(s); }
API uint8_t  api_moveRightToWall       (State* s) { return moveRightToWall(s); }
API uint8_t  api_softDrop              (State* s) { return softDrop(s); }
API uint8_t  api_softDropToFloor       (State* s) { return softDropToFloor(s); }
API uint8_t  api_hardDrop              (State* s) { return hardDrop(s); }
API uint8_t  api_rotateClockwise       (State* s) { return rotateClockwise(s); }
API uint8_t  api_rotateCounterclockwise(State* s) { return rotateCounterclockwise(s); }
API uint8_t  api_rotate180             (State* s) { return rotate180(s); }
API uint8_t  api_hold                  (State* s) { return hold(s); }
API uint8_t  api_noop                  (State* s) { return noop(s); }

API uint8_t  api_addGarbage            (State* s, std::uint8_t lines, std::uint8_t delay) { return addGarbage(s, lines, delay); }

API void     api_toString              (State* s, char* buf, std::uint64_t size) { toString(s, buf, static_cast<std::size_t>(size)); }

API void     api_placeCurrentBlock     (State* s) { placeCurrentBlock(s); }
API void     api_removeCurrentBlock    (State* s) { removeCurrentBlock(s); }
API uint8_t  api_canPlaceCurrentBlock  (State* s) { return canPlaceCurrentBlock(s); }
"""
)

_lib = dl.DynamicLibrary(
    extra_compile_flags=[
        f"-I{CSRC_DIR}",
        "-std=c++17",
        "-O3",
    ],
    watch_files=[
        csrc_path(_ENGINE_HPP),
        csrc_path(_ENGINE_CPP),
    ],
)

_lib.compile_string(
    _WRAPPER_SOURCE,
    functions={
        "api_setSeed": {"argtypes": [dl.void_p, dl.uint32, dl.uint32], "restype": dl.void},
        "api_reset": {"argtypes": [dl.void_p], "restype": dl.void},
        "api_moveLeft": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_moveRight": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_moveLeftToWall": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_moveRightToWall": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_softDrop": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_softDropToFloor": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_hardDrop": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_rotateClockwise": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_rotateCounterclockwise": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_rotate180": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_hold": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_noop": {"argtypes": [dl.void_p], "restype": dl.uint8},
        "api_addGarbage": {"argtypes": [dl.void_p, dl.uint8, dl.uint8], "restype": dl.uint8},
        "api_toString": {"argtypes": [dl.void_p, dl.void_p, dl.uint64], "restype": dl.void},
        "api_placeCurrentBlock": {"argtypes": [dl.void_p], "restype": dl.void},
        "api_removeCurrentBlock": {"argtypes": [dl.void_p], "restype": dl.void},
        "api_canPlaceCurrentBlock": {"argtypes": [dl.void_p], "restype": dl.uint8},
    },
)


_TOSTRING_BUF_SIZE = 2048


def set_seed(state: State, seed: int, garbage_seed: int) -> None:
    """Seed the piece-bag RNG and the garbage RNG."""
    _lib.api_setSeed(ctypes.addressof(state), seed, garbage_seed)


def reset(state: State) -> None:
    """Reset *state* to the initial game state."""
    _lib.api_reset(ctypes.addressof(state))


def move_left(state: State) -> bool:
    """Move the current piece one cell to the left."""
    return bool(_lib.api_moveLeft(ctypes.addressof(state)))


def move_right(state: State) -> bool:
    """Move the current piece one cell to the right."""
    return bool(_lib.api_moveRight(ctypes.addressof(state)))


def move_left_to_wall(state: State) -> bool:
    """Slide the current piece left until it can no longer move."""
    return bool(_lib.api_moveLeftToWall(ctypes.addressof(state)))


def move_right_to_wall(state: State) -> bool:
    """Slide the current piece right until it can no longer move."""
    return bool(_lib.api_moveRightToWall(ctypes.addressof(state)))


def soft_drop(state: State) -> bool:
    """Drop the current piece one row."""
    return bool(_lib.api_softDrop(ctypes.addressof(state)))


def soft_drop_to_floor(state: State) -> bool:
    """Drop the current piece until it lands."""
    return bool(_lib.api_softDropToFloor(ctypes.addressof(state)))


def hard_drop(state: State) -> bool:
    """Hard-drop, lock, clear lines, and spawn the next piece.

    Returns ``True`` if the game is still alive after the drop.
    """
    return bool(_lib.api_hardDrop(ctypes.addressof(state)))


def rotate_cw(state: State) -> bool:
    """Rotate the current piece clockwise (SRS)."""
    return bool(_lib.api_rotateClockwise(ctypes.addressof(state)))


def rotate_ccw(state: State) -> bool:
    """Rotate the current piece counter-clockwise (SRS)."""
    return bool(_lib.api_rotateCounterclockwise(ctypes.addressof(state)))


def rotate_180(state: State) -> bool:
    """Rotate the current piece 180 degrees (SRS)."""
    return bool(_lib.api_rotate180(ctypes.addressof(state)))


def hold(state: State) -> bool:
    """Swap the current piece with the hold slot."""
    return bool(_lib.api_hold(ctypes.addressof(state)))


def noop(state: State) -> bool:
    """No-op action (always succeeds)."""
    return bool(_lib.api_noop(ctypes.addressof(state)))


def add_garbage(state: State, lines: int, delay: int) -> bool:
    """Queue *lines* garbage rows with *delay* steps before they arrive."""
    return bool(_lib.api_addGarbage(ctypes.addressof(state), lines, delay))


def to_string(state: State) -> str:
    """Return a human-readable ASCII rendering of *state*."""
    buf = ctypes.create_string_buffer(_TOSTRING_BUF_SIZE)
    _lib.api_toString(ctypes.addressof(state), ctypes.addressof(buf), _TOSTRING_BUF_SIZE)
    return buf.value.decode()


def place_current_block(state: State) -> None:
    """Stamp the current piece onto the board (without locking)."""
    _lib.api_placeCurrentBlock(ctypes.addressof(state))


def remove_current_block(state: State) -> None:
    """Erase the current piece from the board."""
    _lib.api_removeCurrentBlock(ctypes.addressof(state))


def can_place_current_block(state: State) -> bool:
    """Return ``True`` if the current piece fits at its current position."""
    return bool(_lib.api_canPlaceCurrentBlock(ctypes.addressof(state)))
