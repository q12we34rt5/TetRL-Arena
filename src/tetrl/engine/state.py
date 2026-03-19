from __future__ import annotations

import ctypes
import enum


BOARD_HEIGHT = 32  # total rows
BOARD_WIDTH = 16  # total columns (2-bit cells per 32-bit row)

BOARD_TOP = 9  # first visible row
BOARD_BOTTOM = 28  # last visible row
BOARD_LEFT = 3  # first playfield column
BOARD_RIGHT = 12  # last playfield column

BOARD_FLOOR = BOARD_HEIGHT - BOARD_BOTTOM - 1  # floor wall thickness

BLOCK_SPAWN_X = BOARD_LEFT + 3
BLOCK_SPAWN_Y = BOARD_TOP - 1

GARBAGE_QUEUE_SIZE = 20


class Cell(enum.IntEnum):
    EMPTY = 0b00_000000000000000000000000000000
    BLOCK = 0b01_000000000000000000000000000000
    GARBAGE = 0b11_000000000000000000000000000000


CELL_MASK = 0b11_000000000000000000000000000000


class BlockType(enum.IntEnum):
    NONE = -1
    Z = 0
    L = 1
    O = 2
    S = 3
    I = 4
    J = 5
    T = 6

    @property
    def char(self) -> str:
        return "." if self == BlockType.NONE else self.name


class SpinType(enum.IntEnum):
    NONE = 0
    SPIN = 1
    SPIN_MINI = 2


class State(ctypes.Structure):
    """Binary-compatible mirror of ``struct State`` (tetris.hpp)."""

    _fields_ = [
        ("board", ctypes.c_uint32 * BOARD_HEIGHT),
        ("is_alive", ctypes.c_uint8),
        ("next", ctypes.c_int8 * 14),
        ("hold", ctypes.c_int8),
        ("has_held", ctypes.c_uint8),
        ("current", ctypes.c_int8),
        ("orientation", ctypes.c_uint8),
        ("x", ctypes.c_int8),
        ("y", ctypes.c_int8),
        ("seed", ctypes.c_uint32),
        ("srs_index", ctypes.c_int8),
        ("piece_count", ctypes.c_uint32),
        ("was_last_rotation", ctypes.c_uint8),
        ("spin_type", ctypes.c_uint8),
        ("perfect_clear", ctypes.c_uint8),
        ("back_to_back_count", ctypes.c_int32),
        ("combo_count", ctypes.c_int32),
        ("lines_cleared", ctypes.c_uint16),
        ("attack", ctypes.c_uint16),
        ("lines_sent", ctypes.c_uint16),
        ("total_lines_cleared", ctypes.c_uint32),
        ("total_attack", ctypes.c_uint32),
        ("total_lines_sent", ctypes.c_uint32),
        ("garbage_seed", ctypes.c_uint32),
        ("garbage_queue", ctypes.c_uint8 * GARBAGE_QUEUE_SIZE),
        ("garbage_delay", ctypes.c_uint8 * GARBAGE_QUEUE_SIZE),
        ("max_garbage_spawn", ctypes.c_uint8),
        ("garbage_blocking", ctypes.c_uint8),
    ]

    def __init__(self, **kwargs):
        kwargs.setdefault("max_garbage_spawn", 6)
        kwargs.setdefault("garbage_blocking", True)
        super().__init__(**kwargs)


# TODO: add ops
