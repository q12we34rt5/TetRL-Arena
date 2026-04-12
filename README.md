# TetRL-Arena

TetRL-Arena is a Tetris reinforcement-learning toolkit built around a native C++ engine and a Gymnasium-compatible Python environment.

Current package entry point:

- `tetrl/Step-v0`: step-based Tetris environment with the default 66-channel observation and lock-based reward.

## Requirements

- Python 3.9+
- A C++17 compiler available on `PATH`
  - macOS: `clang++` or `g++`
  - Linux: `g++` or `clang++`

The Python package JIT-compiles native wrappers at runtime, so a working compiler is required.

## Install

```bash
pip install -e .
```

From a local checkout, you can also run the package directly without installing it first:

```bash
PYTHONPATH=src python
```

## Quick Start

```python
import gymnasium
import tetrl

env = gymnasium.make("tetrl/Step-v0", render_mode="ansi")

obs, info = env.reset(seed=42)
print(obs.shape)  # (66, 20, 10)

obs, reward, terminated, truncated, info = env.step(3)  # HARD_DROP
print(reward, terminated, truncated, info)
print(env.render())
```

You can also construct the environment directly:

```python
from tetrl.envs.step import StepEnv, StepEnvConfig

env = StepEnv(
    config=StepEnvConfig(piece_life=20, auto_drop=True),
    render_mode="ansi",
)
```

## Project Layout

- `src/tetrl/csrc/`: bundled native engine and step-environment headers/sources
- `src/tetrl/dynamic_library/`: runtime C/C++ compilation and ctypes binding helpers
- `src/tetrl/engine/`: low-level Python bindings for the core Tetris engine
- `src/tetrl/envs/step/`: step-based environment bindings, plugins, defaults, and Gymnasium env

## Extensibility

`StepEnv` supports custom observation and reward plugins:

- Python plugins via the `FeaturePlugin` and `RewardPlugin` interfaces
- Native plugins via `CppFeature` and `CppReward`

This allows the environment loop to stay in Python while performance-sensitive feature extraction and reward logic can run in C++.
