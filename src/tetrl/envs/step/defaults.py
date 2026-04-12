"""
Built-in default plugins for the step-based environment.

These are ready-to-use :class:`CppFeature` / :class:`CppReward` instances
that provide a sensible baseline observation and reward without any user
C++ code.  They are used by :class:`StepEnv` when no plugins are supplied,
and by the ``tetrl/Step-v0`` gymnasium registration.

Default feature -- ``default_feature()``
--------------------------------------
66-channel image with shape ``(66, 20, 10)`` (``float32``).
 0 - 3   Frame history (4 snapshots; newest includes the active piece,
         reset bootstrap frames may be board-only)
 4       Board top mask
 5       Board holes mask
 6       Current piece on empty board
 7 - 10  Current orientation (one-hot 0/1/2/3)
11 - 17  Current piece type (one-hot Z/L/O/S/I/J/T)
18 - 24  Hold type (one-hot; all-zero when empty)
25       Has held
26 - 60  Next[0..4] type (5 x 7 one-hot)
61       Lifetime (normalised remaining moves before forced drop, broadcast to plane)
62       Shadow (hard-drop ghost on empty board)
63       Pending garbage (per-row value = max(10 - delay, 1) / 10; 0 = none)
64       Back-to-back flag
65       Combo count / 12, clipped to 1

All values in ``[0, 1]``.

Default reward -- ``default_reward()``
--------------------------------------
Lock-based reward with direct attack incentive:
 Non-locking step      0
 Locking top-out      -20
 Lock survival        +1
 Line clear reward    base_by_lines * height_multiplier * 2
 Attack reward        +10 * attack
 Low lock-height      placement_height_bonus / 3
 New holes penalty    -log(delta + 1) * 2
 New stack voids      -(delta / 2)
"""

from __future__ import annotations

import numpy as np

from .feature import CppFeature
from .reward import CppReward

# Feature - 66-channel 20x10 image
_NUM_CHANNELS = 66
_ROWS = 20
_COLS = 10

_DEFAULT_FEATURE_SRC = r"""
#include <algorithm>
using namespace ops;

static constexpr int ROWS       = BOARD_BOTTOM - BOARD_TOP  + 1; // 20
static constexpr int COLS       = BOARD_RIGHT  - BOARD_LEFT + 1; // 10
static constexpr int CH         = ROWS * COLS;                   // 200
static constexpr int N_FRAMES   = 4;
static constexpr int N_ROTS     = 4;
static constexpr int N_TYPES    = 7;
static constexpr int N_NEXT     = 5;
static constexpr int N_CHANNELS =
    N_FRAMES + 1 + 1 + 1 + N_ROTS + N_TYPES + N_TYPES + 1
    + N_NEXT * N_TYPES + 1 + 1 + 1 + 1 + 1;                     // 66
static constexpr int FEATURE_SIZE = N_CHANNELS * CH;            // 13200
static constexpr int VIS_TOP = BOARD_TOP;

struct FeatureContext {
    float frames[N_FRAMES][CH];
};

static float ones_buf[CH];
static bool  g_init = false;

static void ensure_init() {
    if (!g_init) {
        for (int i = 0; i < CH; ++i) ones_buf[i] = 1.0f;
        g_init = true;
    }
}

inline void board_to_channel(const Board& board, float* ch) {
    for (int r = VIS_TOP; r <= BOARD_BOTTOM; ++r)
        for (int c = 0; c < COLS; ++c)
            ch[(r - VIS_TOP) * COLS + c] =
                (getCell(board, BOARD_LEFT + c, r) != Cell::EMPTY) ? 1.0f : 0.0f;
}

inline void fill_ones(float* ch)  { std::memcpy(ch, ones_buf, sizeof(float) * CH); }
inline void fill_zeros(float* ch) { std::memset(ch, 0, sizeof(float) * CH); }

inline void fill_value(float* ch, float v) {
    for (int i = 0; i < CH; ++i) ch[i] = v;
}

inline void make_board_features(State* s, float* top, float* holes) {
    int col_hit[COLS] = {};
    for (int r = VIS_TOP; r <= BOARD_BOTTOM; ++r)
        for (int c = 0; c < COLS; ++c) {
            bool occupied = (getCell(s->board, BOARD_LEFT + c, r) != Cell::EMPTY);
            col_hit[c] |= occupied ? 1 : 0;
            int idx = (r - VIS_TOP) * COLS + c;
            top[idx]   = static_cast<float>(col_hit[c] != 0);
            holes[idx] = static_cast<float>(!occupied && col_hit[c] != 0);
        }
}

inline void make_current_piece(State* s, float* piece_ch, float* rot_ch) {
    Board tmp = {};
    placePiece(tmp, getPiece(s->current, s->orientation), s->x, s->y);
    board_to_channel(tmp, piece_ch);

    for (int i = 0; i < N_ROTS; ++i) {
        float* ch = rot_ch + i * CH;
        if (i == s->orientation) fill_ones(ch);
        else                     fill_zeros(ch);
    }
}

inline void piece_type_one_hot(float* ch, PieceType type) {
    for (int i = 0; i < N_TYPES; ++i) {
        float* dst = ch + i * CH;
        if (type != PieceType::NONE && i == static_cast<int>(type))
            fill_ones(dst);
        else
            fill_zeros(dst);
    }
}

inline void make_shadow(State* s, float* ch) {
    State tmp;
    std::memcpy(&tmp, s, sizeof(State));
    while (softDrop(&tmp)) {}
    std::memset(tmp.board.data, 0, sizeof(tmp.board.data));
    placeCurrentPiece(&tmp);
    board_to_channel(tmp.board, ch);
}

inline void make_garbage(State* s, float* ch) {
    fill_zeros(ch);
    int row = ROWS - 1;
    for (int i = 0; i < GARBAGE_QUEUE_SIZE && row >= 0; ++i) {
        if (s->garbage_queue[i] == 0) break;
        int length = s->garbage_queue[i];
        int delay  = s->garbage_delay[i];
        int raw    = 10 - delay;
        if (raw < 1) raw = 1;
        float val  = static_cast<float>(raw) / 10.0f;
        for (; row >= 0 && length > 0; --row, --length)
            for (int c = 0; c < COLS; ++c)
                ch[row * COLS + c] = val;
    }
}

inline void compute(Context* env_ctx, FeatureContext* feature_ctx, float* out) {
    ensure_init();
    State* s = &env_ctx->state;
    float* p = out;

    std::memcpy(p, feature_ctx->frames, sizeof(float) * N_FRAMES * CH);
    p += N_FRAMES * CH;

    make_board_features(s, p, p + CH);
    p += 2 * CH;

    make_current_piece(s, p, p + CH);
    p += (1 + N_ROTS) * CH;

    piece_type_one_hot(p, s->current);
    p += N_TYPES * CH;

    piece_type_one_hot(p, s->hold);
    p += N_TYPES * CH;

    if (s->has_held) fill_ones(p); else fill_zeros(p);
    p += CH;

    for (int i = 0; i < N_NEXT; ++i) {
        piece_type_one_hot(p, s->next[i]);
        p += N_TYPES * CH;
    }

    float lt = (env_ctx->lifetime - 1 < 10)
             ? static_cast<float>(env_ctx->lifetime - 1) / 10.0f
             : 1.0f;
    fill_value(p, lt);
    p += CH;

    make_shadow(s, p);
    p += CH;

    make_garbage(s, p);
    p += CH;

    float b2b_val = (s->back_to_back_count > 0) ? 1.0f : 0.0f;
    fill_value(p, b2b_val);
    p += CH;

    float combo_val = std::clamp(static_cast<float>(s->combo_count) / 12.0f, 0.0f, 1.0f);
    fill_value(p, combo_val);
}

API int  feature_context_size() { return static_cast<int>(sizeof(FeatureContext)); }
API int  feature_size()         { return FEATURE_SIZE; }

API void feature_reset(Context* env_ctx, void* plugin_ctx) {
    ensure_init();
    State* s = &env_ctx->state;
    auto* feature_ctx = static_cast<FeatureContext*>(plugin_ctx);
    for (int i = 0; i < N_FRAMES; ++i)
        board_to_channel(s->board, feature_ctx->frames[i]);
}

API void feature_step(Context* env_ctx, Info*, void* plugin_ctx, float* out) {
    State* s = &env_ctx->state;
    auto* feature_ctx = static_cast<FeatureContext*>(plugin_ctx);

    placeCurrentPiece(s);
    for (int i = N_FRAMES - 2; i >= 0; --i)
        std::memcpy(feature_ctx->frames[i + 1], feature_ctx->frames[i], sizeof(float) * CH);
    board_to_channel(s->board, feature_ctx->frames[0]);
    removeCurrentPiece(s);

    compute(env_ctx, feature_ctx, out);
}
"""


def default_feature() -> CppFeature:
    """Create the default 66-channel image feature plugin."""
    import gymnasium

    return CppFeature(
        _DEFAULT_FEATURE_SRC,
        observation_space=gymnasium.spaces.Box(
            low=0.0,
            high=1.0,
            shape=(_NUM_CHANNELS, _ROWS, _COLS),
            dtype=np.float32,
        ),
        unpack=lambda buf: buf.reshape(_NUM_CHANNELS, _ROWS, _COLS),
    )


# Reward - lock-based attack shaping with row-mask board statistics
_DEFAULT_REWARD_SRC = r"""
using namespace ops;

static constexpr int ROWS = BOARD_BOTTOM - BOARD_TOP + 1;  // 20
static constexpr int PLAYFIELD_COLS = BOARD_RIGHT - BOARD_LEFT + 1;  // 10

static constexpr float line_clear_base[] = {0.0f, 3.0f, 8.0f, 14.0f, 21.0f};

static constexpr float placement_height_multiplier[] = {
//  h: 0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19    20
    3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 2.0f, 2.0f, 2.0f, 1.5f, 1.5f, 1.0f, 1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.3f, 0.3f, 0.3f
};

static constexpr float placement_height_bonus[] = {
//  h: 0     1     2     3     4     5     6     7     8     9    10    11    12    13    14    15    16    17    18    19    20
    6.0f, 5.5f, 5.0f, 4.5f, 4.0f, 3.5f, 3.0f, 2.5f, 2.0f, 1.5f, 1.0f, 1.0f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.4f, 0.4f, 0.4f, 0.4f
};

struct RewardContext {
    State previous_state;
    int previous_hole_count;
    int previous_stack_void_count;
};

static constexpr Row make_playfield_occupied_mask() {
    Row mask = 0;
    for (int col = BOARD_LEFT; col <= BOARD_RIGHT; ++col) {
        mask |= shift(static_cast<Row>(Cell::BLOCK), col);
    }
    return mask;
}

static constexpr Row PLAYFIELD_OCCUPIED_MASK = make_playfield_occupied_mask();

static inline int bit_count_u32(Row bits) {
#if defined(__clang__) || defined(__GNUC__)
    return __builtin_popcount(static_cast<unsigned int>(bits));
#else
    int count = 0;
    while (bits != 0) {
        bits &= (bits - 1);
        ++count;
    }
    return count;
#endif
}

static inline float lookup_clamped(const float* table, int height) {
    if (height < 0) {
        height = 0;
    } else if (height > ROWS) {
        height = ROWS;
    }
    return table[height];
}

static int count_piece_leading_empty_rows(PieceType type, std::uint8_t orientation) {
    const Piece& piece = getPiece(type, orientation);
    int leading_empty_rows = 0;
    for (int row = 0; row < 4; ++row) {
        bool row_has_cell = false;
        for (int col = 0; col < 4; ++col) {
            if (getCell(piece, col, row) != Cell::EMPTY) {
                row_has_cell = true;
                break;
            }
        }
        if (row_has_cell) {
            break;
        }
        leading_empty_rows++;
    }
    return leading_empty_rows;
}

static int compute_lock_height(std::int8_t piece_y, int leading_empty_rows) {
    return ROWS - (piece_y + leading_empty_rows - BOARD_TOP);
}

static inline Row row_occupied_mask(const Board& board, int row) {
    return board.data[row] & PLAYFIELD_OCCUPIED_MASK;
}

static int compute_hole_count(const Board& board) {
    int hole_count = 0;
    Row filled_above_mask = 0;
    for (int row = BOARD_TOP; row <= BOARD_BOTTOM; ++row) {
        const Row occupied_mask = row_occupied_mask(board, row);
        const Row hole_mask = filled_above_mask & ~occupied_mask & PLAYFIELD_OCCUPIED_MASK;
        hole_count += bit_count_u32(hole_mask);
        filled_above_mask |= occupied_mask;
    }
    return hole_count;
}

static int compute_stack_void_count(const Board& board) {
    int stack_void_count = 0;
    bool stack_started = false;
    for (int row = BOARD_TOP; row <= BOARD_BOTTOM; ++row) {
        const Row occupied_mask = row_occupied_mask(board, row);
        if (!stack_started && occupied_mask != 0) {
            stack_started = true;
        }
        if (stack_started) {
            stack_void_count += PLAYFIELD_COLS - bit_count_u32(occupied_mask);
        }
    }
    return stack_void_count;
}

static bool is_locking_step(const Info* info) {
    return static_cast<Action>(info->action_id) == Action::HARD_DROP
        || info->forced_hard_drop;
}

API int reward_context_size() { return static_cast<int>(sizeof(RewardContext)); }

API void reward_reset(Context* env_ctx, void* plugin_ctx) {
    State* state = &env_ctx->state;
    auto* reward_ctx = static_cast<RewardContext*>(plugin_ctx);

    std::memcpy(&reward_ctx->previous_state, state, sizeof(State));
    reward_ctx->previous_hole_count = compute_hole_count(state->board);
    reward_ctx->previous_stack_void_count = compute_stack_void_count(state->board);
}

API float reward_step(Context* env_ctx, Info* info, void* plugin_ctx) {
    State* state = &env_ctx->state;
    auto* reward_ctx = static_cast<RewardContext*>(plugin_ctx);

    if (!is_locking_step(info)) {
        std::memcpy(&reward_ctx->previous_state, state, sizeof(State));
        return 0.0f;
    }

    if (!state->is_alive) {
        return -20.0f;
    }

    PieceType locking_piece_type = reward_ctx->previous_state.current;
    std::uint8_t locking_piece_orientation = reward_ctx->previous_state.orientation;
    std::int8_t locking_piece_y = reward_ctx->previous_state.y;

    {
        State simulated_lock_state;
        std::memcpy(&simulated_lock_state, &reward_ctx->previous_state, sizeof(State));
        while (softDrop(&simulated_lock_state)) {}
        locking_piece_orientation = simulated_lock_state.orientation;
        locking_piece_y = simulated_lock_state.y;
    }

    const int leading_empty_rows =
        count_piece_leading_empty_rows(locking_piece_type, locking_piece_orientation);
    const int lock_height = compute_lock_height(locking_piece_y, leading_empty_rows);

    float reward = 1.0f;

    int cleared_rows = static_cast<int>(state->lines_cleared);
    if (cleared_rows < 0) {
        cleared_rows = 0;
    } else if (cleared_rows > 4) {
        cleared_rows = 4;
    }
    reward += line_clear_base[cleared_rows]
            * lookup_clamped(placement_height_multiplier, lock_height)
            * 2.0f;

    reward += static_cast<float>(state->attack) * 10.0f;
    reward += lookup_clamped(placement_height_bonus, lock_height) / 3.0f;

    const int current_hole_count = compute_hole_count(state->board);
    const int new_hole_count_delta =
        current_hole_count - reward_ctx->previous_hole_count;
    if (new_hole_count_delta > 0) {
        reward -= std::log(static_cast<float>(new_hole_count_delta + 1)) * 2.0f;
    }

    const int current_stack_void_count = compute_stack_void_count(state->board);
    const int new_stack_void_delta =
        current_stack_void_count - reward_ctx->previous_stack_void_count;
    if (new_stack_void_delta > 0) {
        reward -= static_cast<float>(new_stack_void_delta) / 2.0f;
    }

    std::memcpy(&reward_ctx->previous_state, state, sizeof(State));
    reward_ctx->previous_hole_count = current_hole_count;
    reward_ctx->previous_stack_void_count = current_stack_void_count;

    return reward;
}
"""


def default_reward() -> CppReward:
    """Create the default lock-based attack reward plugin."""
    return CppReward(_DEFAULT_REWARD_SRC)
