"""
TetRL -- Tetris reinforcement-learning toolkit.

Importing this package registers the following gymnasium environments:

* ``tetrl/Step-v0`` -- step-based env with default plugins
  (66-channel feature tensor, lock-based attack reward).
"""

__all__ = []

import gymnasium

gymnasium.register(
    id="tetrl/Step-v0",
    entry_point="tetrl.envs.step.env:StepEnv",
    # feature=None and reward=None -> defaults are created automatically.
)
