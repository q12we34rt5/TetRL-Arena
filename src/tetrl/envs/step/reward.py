"""
Reward plugin for the step-based environment.

Provides :class:`RewardPlugin` -- the abstract contract every reward
calculator must fulfil -- and :class:`CppReward`, a concrete implementation
that JIT-compiles user-supplied C++ code via :mod:`~tetrl.dynamic_library`.

Both ``tetris.hpp`` functions and the step-env types (``Info``,
``Action``, ...) are available in user C++ code.
"""

from __future__ import annotations

import ctypes
import os
from abc import ABC, abstractmethod
from typing import Sequence

from ... import dynamic_library as dl
from ...native_layout import CSRC_DIR, csrc_path
from .native import StepEnvContext, StepInfo

_ENGINE_CPP = "engine/tetris.cpp"
_ENGINE_HPP = "engine/tetris.hpp"
_STEP_HPP = "envs/step/step.hpp"


class RewardPlugin(ABC):
    """Abstract base class for reward plugins.

    Subclass this to define custom reward functions for the
    step-based Tetris RL environment.  The environment calls
    :meth:`reset` once at the start of each episode and :meth:`step`
    after every action.
    """

    @abstractmethod
    def reset(self, ctx: StepEnvContext) -> None:
        """Called on environment reset.  Initialize internal state.

        Parameters
        ----------
        ctx:
            The environment context after reset.
        """
        ...

    @abstractmethod
    def step(self, ctx: StepEnvContext, step_info: StepInfo) -> float:
        """Compute the reward after a step.

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
        float
            The scalar reward.
        """
        ...


class CppReward(RewardPlugin):
    """Reward plugin backed by JIT-compiled C++ code.

    ``tetris.cpp`` and ``step.hpp`` are compiled together with the user
    source, so all engine types/functions **and** step-env types
    (``Info``, ``Action``, ...) are available without additional includes.

    Required C++ functions
    ----------------------
    The source must define the following ``API``-prefixed functions::

        API int   reward_context_size()                        // bytes; 0 = stateless
        API void  reward_reset(Context* ctx, void* plugin_ctx)
        API float reward_step(Context* ctx, Info* info,
                              void* plugin_ctx)

    ``reward_context_size`` returns the byte size of the user-defined
    reward plugin context struct (return ``0`` if no per-instance state
    is needed). Python allocates a buffer of that size for each
    :class:`CppReward` instance and passes it as ``plugin_ctx`` on every
    call.

    ``reward_reset`` is called once per episode.  ``reward_step`` is
    called after every action and must return a scalar ``float``.

    Parameters
    ----------
    source:
        C++ source code.
    extra_compile_flags:
        Additional flags passed to the C++ compiler.
    watch_files:
        Extra header / source files whose content should invalidate
        the compilation cache when changed.

    Examples
    --------
    Stateless:

    >>> reward = CppReward(r'''
    ...     API int   reward_context_size() { return 0; }
    ...     API void  reward_reset(Context*, void*) {}
    ...     API float reward_step(Context* ctx, Info* info, void*) {
    ...         State* s = &ctx->state;
    ...         float r = static_cast<float>(s->attack);
    ...         if (!s->is_alive) r -= 5.0f;
    ...         return r;
    ...     }
    ... ''')

    Stateful (penalises back-to-back breaks):

    >>> reward = CppReward(r'''
    ...     struct RewardContext { int32_t prev_b2b; };
    ...     API int   reward_context_size() { return sizeof(RewardContext); }
    ...     API void  reward_reset(Context* ctx, void* plugin_ctx) {
    ...         State* s = &ctx->state;
    ...         auto* c = static_cast<RewardContext*>(plugin_ctx);
    ...         c->prev_b2b = s->back_to_back_count;
    ...     }
    ...     API float reward_step(Context* ctx, Info*, void* plugin_ctx) {
    ...         State* s = &ctx->state;
    ...         auto* c = static_cast<RewardContext*>(plugin_ctx);
    ...         float r = static_cast<float>(s->lines_cleared);
    ...         if (c->prev_b2b >= 0 && s->back_to_back_count == -1)
    ...             r -= 3.0f;
    ...         c->prev_b2b = s->back_to_back_count;
    ...         if (!s->is_alive) r -= 5.0f;
    ...         return r;
    ...     }
    ... ''')
    """

    def __init__(
        self,
        source: str,
        *,
        extra_compile_flags: Sequence[str] | None = None,
        watch_files: Sequence[str | os.PathLike] | None = None,
    ) -> None:
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
                "reward_context_size": {"argtypes": [], "restype": dl.int32},
                "reward_reset": {"argtypes": [dl.void_p, dl.void_p], "restype": dl.void},
                "reward_step": {"argtypes": [dl.void_p, dl.void_p, dl.void_p], "restype": dl.float},
            },
        )

        # Allocate per-instance context buffer.
        self._ctx_size: int = int(self._lib.reward_context_size())
        self._ctx_buf = ctypes.create_string_buffer(self._ctx_size) if self._ctx_size > 0 else None
        self._ctx_ptr: int = ctypes.addressof(self._ctx_buf) if self._ctx_buf is not None else 0

    def reset(self, ctx: StepEnvContext) -> None:
        self._lib.reward_reset(ctypes.addressof(ctx), self._ctx_ptr)

    def step(self, ctx: StepEnvContext, step_info: StepInfo) -> float:
        return float(
            self._lib.reward_step(
                ctypes.addressof(ctx),
                ctypes.addressof(step_info),
                self._ctx_ptr,
            )
        )

    @property
    def context_size(self) -> int:
        """Byte size of the C++ reward plugin context (0 = stateless)."""
        return self._ctx_size

    def close(self) -> None:
        """Release the compiled shared library."""
        self._lib.close()

    def __repr__(self) -> str:
        return f"CppReward(context_size={self._ctx_size})"
