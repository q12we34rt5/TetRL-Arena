"""Project-local helpers for locating native sources under csrc."""

from __future__ import annotations

from pathlib import Path

__all__ = ["CSRC_DIR", "csrc_path"]


_TETRL_DIR = Path(__file__).resolve().parent

CSRC_DIR = (_TETRL_DIR / "csrc").resolve()


def csrc_path(relative: str) -> Path:
    """Return an absolute filesystem path for a project-relative csrc path."""
    return CSRC_DIR.joinpath(*relative.split("/")).resolve()
