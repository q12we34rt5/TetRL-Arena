"""
Gymnasium environment for the step-based Tetris engine.

Wraps :mod:`~tetrl.envs.step.native` together with a
:class:`~tetrl.envs.step.feature.FeaturePlugin` (observations) and a
:class:`~tetrl.envs.step.reward.RewardPlugin` (rewards) into a standard
:class:`gymnasium.Env`.

Examples
--------
>>> from tetrl.envs.step import CppFeature, CppReward, StepEnv
>>>
>>> feature = CppFeature(r'''
...     using namespace ops;
...     API int  feature_context_size() { return 0; }
...     API int  feature_size() { return 200; }
...     API void feature_reset(Context*, void*) {}
...     API void feature_step(Context* ctx, Info*, void*,
...                           float* out) {
...         State* s = &ctx->state;
...         for (int r = 0; r < 20; ++r)
...             for (int c = 0; c < 10; ++c)
...                 out[r * 10 + c] = (getCell(s->board,
...                     BOARD_LEFT + c, BOARD_TOP + r) != Cell::EMPTY)
...                     ? 1.0f : 0.0f;
...     }
... ''')
>>>
>>> reward = CppReward(r'''
...     API int   reward_context_size() { return 0; }
...     API void  reward_reset(Context*, void*) {}
...     API float reward_step(Context* ctx, Info*, void*) {
...         State* s = &ctx->state;
...         return static_cast<float>(s->attack)
...              - (!s->is_alive ? 5.0f : 0.0f);
...     }
... ''')
>>>
>>> env = StepEnv(feature=feature, reward=reward)
>>> observation, info = env.reset(seed=42)
>>> observation, reward, term, trunc, info = env.step(3)  # HARD_DROP
"""

from __future__ import annotations

from typing import Any, SupportsFloat

import gymnasium

from .native import (
    Action,
    N_ACTIONS,
    StepEnvConfig,
    StepEnvContext,
    env_reset,
    env_set_config,
    env_set_seed,
    env_step,
)
from .feature import FeaturePlugin
from .reward import RewardPlugin


class StepEnv(gymnasium.Env):
    """Gymnasium environment for per-step Tetris control.

    Each call to :meth:`step` executes **one** low-level action
    (move, rotate, drop, hold, noop, ...).  Gravity and forced drops are
    handled transparently by the C++ engine according to ``config``.

    Parameters
    ----------
    feature:
        A :class:`FeaturePlugin` that produces observations.  When
        ``None``, uses :func:`~tetrl.envs.step.defaults.default_feature`
        (a 66-channel ``(66, 20, 10)`` feature tensor).
    reward:
        A :class:`RewardPlugin` that computes scalar rewards.  When
        ``None``, uses :func:`~tetrl.envs.step.defaults.default_reward`
        (lock-based attack shaping).
    config:
        Engine configuration (block lifetime, auto-drop gravity).
        Defaults to ``StepEnvConfig()`` (``block_life=20, auto_drop=True``).
    max_steps:
        If positive, the episode is *truncated* after this many steps
        (the ``truncated`` flag is set but ``terminated`` stays ``False``).
        ``0`` means no truncation limit.
    render_mode:
        ``"ansi"`` returns the board as a multi-line string.
        ``None`` disables rendering.

    Attributes
    ----------
    observation_space : gymnasium.spaces.Space
        Determined by *feature*.
    action_space : gymnasium.spaces.Discrete
        The 12 actions defined in :class:`Action`.
    """

    metadata = {
        "render_modes": ["ansi"],
        # Declared for Gymnasium's metadata checks; ansi rendering is not timed here.
        "render_fps": 1,
    }

    def __init__(
        self,
        *,
        feature: FeaturePlugin | None = None,
        reward: RewardPlugin | None = None,
        config: StepEnvConfig | None = None,
        max_steps: int = 0,
        render_mode: str | None = None,
    ) -> None:
        super().__init__()

        if feature is None:
            from .defaults import default_feature

            feature = default_feature()
        if reward is None:
            from .defaults import default_reward

            reward = default_reward()

        self._feature = feature
        self._reward = reward
        self._max_steps = max_steps
        self.render_mode = render_mode

        # Internal engine context.
        self._ctx = StepEnvContext(config=config or StepEnvConfig())
        env_set_seed(self._ctx, 1, 1)

        # Gymnasium spaces.
        self.observation_space = self._feature.observation_space()
        self.action_space = gymnasium.spaces.Discrete(N_ACTIONS)

        # Episode bookkeeping.
        self._steps: int = 0
        self._needs_reset: bool = True

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[Any, dict[str, Any]]:
        """Reset the environment and return ``(observation, info)``.

        Parameters
        ----------
        seed:
            Optional RNG seed for reproducibility.  Passing the same
            seed produces the same piece sequence and garbage pattern.
            If ``None``, the existing RNG state continues to be used.
        options:
            ``"config"`` -- a :class:`StepEnvConfig` to apply before reset.
        """
        super().reset(seed=seed, options=options)

        opts = options or {}

        # Apply config override if provided.
        if "config" in opts:
            env_set_config(self._ctx, opts["config"])

        # Seed - draw engine seeds from the Gymnasium np_random PRNG.
        engine_seed = int(self.np_random.integers(1, 2**32))
        garbage_seed = int(self.np_random.integers(1, 2**32))
        env_set_seed(self._ctx, engine_seed, garbage_seed)

        # Engine reset.
        env_reset(self._ctx)

        # Plugin reset.
        self._reward.reset(self._ctx)
        observation = self._feature.reset(self._ctx)

        self._steps = 0
        self._needs_reset = False

        return observation, self._make_info()

    def step(
        self,
        action: int | Action,
    ) -> tuple[Any, SupportsFloat, bool, bool, dict[str, Any]]:
        """Execute one action and return the standard 5-tuple.

        Returns
        -------
        observation
            From the feature plugin.
        reward : float
            From the reward plugin.
        terminated : bool
            ``True`` when the game is over (``is_alive == 0``).
        truncated : bool
            ``True`` when ``max_steps`` is reached.
        info : dict
            Extra diagnostics (see :meth:`_make_info`).
        """
        if self._needs_reset:
            raise RuntimeError("Environment must be reset before calling step(). Call env.reset() first.")

        step_info = env_step(self._ctx, int(action))
        self._steps += 1

        observation = self._feature.step(self._ctx, step_info)
        reward = float(self._reward.step(self._ctx, step_info))

        terminated = not bool(self._ctx.state.is_alive)
        truncated = self._max_steps > 0 and self._steps >= self._max_steps and not terminated

        if terminated or truncated:
            self._needs_reset = True

        info = self._make_info(step_info=step_info)
        return observation, reward, terminated, truncated, info

    def render(self) -> str | None:
        """Render the current board state.

        Only ``render_mode="ansi"`` is supported -- returns an ASCII string.
        """
        if self.render_mode == "ansi":
            from ...engine.native import to_string

            return to_string(self._ctx.state)
        return None

    def close(self) -> None:
        """Release plugin resources."""
        if hasattr(self._feature, "close"):
            self._feature.close()
        if hasattr(self._reward, "close"):
            self._reward.close()

    def send_garbage(self, lines: int, delay: int = 0) -> bool:
        """Queue garbage lines to be received by this environment."""
        from ...engine.native import add_garbage

        return add_garbage(self._ctx.state, lines, delay)

    @property
    def state(self) -> StepEnvContext:
        """Low-level engine context for advanced users."""
        return self._ctx

    @property
    def config(self) -> StepEnvConfig:
        """Current engine configuration."""
        return self._ctx.config

    @property
    def steps(self) -> int:
        """Number of steps taken in the current episode."""
        return self._steps

    def _make_info(self, *, step_info=None) -> dict[str, Any]:
        info: dict[str, Any] = {}
        if step_info is not None:
            info["action_id"] = int(step_info.action_id)
            info["action_success"] = bool(step_info.action_success)
            info["forced_hard_drop"] = bool(step_info.forced_hard_drop)
        return info
