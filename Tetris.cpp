#include "Tetris.hpp"

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <utility>

#include <cassert>

template<template<int...> class Container, int n, int ...args>
struct IndexGenerator : IndexGenerator<Container, n - 1, n - 1, args...> {};
template<template<int...> class Container, int ...args>
struct IndexGenerator<Container, 0, args...> { using result = Container<args...>; };
template<int ...args>
struct Wrapper {
    template<int height, int padding, uint32_t row, uint32_t wall>
    struct BoardInitializer {
        template<bool condition, typename x> struct If {};
        template<typename x> struct If<true, x> { static constexpr uint32_t value = wall; };
        template<typename x> struct If<false, x> { static constexpr uint32_t value = row; };

        static const Rows<height> board;
    };
};
template<int ...args>
template<int height, int padding, uint32_t row, uint32_t wall>
const Rows<height> Wrapper<args...>::template BoardInitializer<height, padding, row, wall>::board = {
    Wrapper<args...>::template BoardInitializer<height, padding, row, wall>::template If<(/*args < padding || */args >= height - padding), int>::value...
};

inline static SRSKickData getSRSKickData(BlockType type, int8_t orientation, Rotation rot) {
    // [<block-orientation>][<rotate-direction>][<srs-index>]
    static const SRSKickData::Kick JLSTZ[4][2][5] = {{
            {{ 0,  0}, {-1,  0}, {-1,  1}, { 0, -2}, {-1, -2}},          // 0 -> 1
            {{ 0,  0}, { 1,  0}, { 1,  1}, { 0, -2}, { 1, -2}},          // 0 -> 3
        }, {
            {{ 0,  0}, { 1,  0}, { 1, -1}, { 0,  2}, { 1,  2}},          // 1 -> 2
            {{ 0,  0}, { 1,  0}, { 1, -1}, { 0,  2}, { 1,  2}},          // 1 -> 0
        }, {
            {{ 0,  0}, { 1,  0}, { 1,  1}, { 0, -2}, { 1, -2}},          // 2 -> 3
            {{ 0,  0}, {-1,  0}, {-1,  1}, { 0, -2}, {-1, -2}},          // 2 -> 1
        }, {
            {{ 0,  0}, {-1,  0}, {-1, -1}, { 0,  2}, {-1,  2}},          // 3 -> 0
            {{ 0,  0}, {-1,  0}, {-1, -1}, { 0,  2}, {-1,  2}},          // 3 -> 2
        }
    };
    static const SRSKickData::Kick JLSTZ180[4][1][6] = {{
            {{ 0,  0}, { 0,  1}, { 1,  1}, {-1,  1}, { 1, 0}, {-1,  0}}, // 0 -> 2
        }, {
            {{ 0,  0}, { 1,  0}, { 1,  2}, { 1,  1}, { 0, 2}, { 0,  1}}, // 1 -> 3
        }, {
            {{ 0,  0}, { 0, -1}, {-1, -1}, { 1, -1}, {-1, 0}, { 1,  0}}, // 2 -> 0
        }, {
            {{ 0,  0}, {-1,  0}, {-1,  2}, {-1,  1}, { 0, 2}, { 0,  1}}, // 3 -> 1
        }
    };
    static const SRSKickData::Kick I[4][2][5] = {{
            {{ 0,  0}, {-2,  0}, { 1,  0}, {-2, -1}, { 1,  2}},          // 0 -> 1
            {{ 0,  0}, {-1,  0}, { 2,  0}, {-1,  2}, { 2, -1}},          // 0 -> 3
        }, {
            {{ 0,  0}, {-1,  0}, { 2,  0}, {-1,  2}, { 2, -1}},          // 1 -> 2
            {{ 0,  0}, { 2,  0}, {-1,  0}, { 2,  1}, {-1, -2}},          // 1 -> 0
        }, {
            {{ 0,  0}, { 2,  0}, {-1,  0}, { 2,  1}, {-1, -2}},          // 2 -> 3
            {{ 0,  0}, { 1,  0}, {-2,  0}, { 1, -2}, {-2,  1}},          // 2 -> 1
        }, {
            {{ 0,  0}, { 1,  0}, {-2,  0}, { 1, -2}, {-2,  1}},          // 3 -> 0
            {{ 0,  0}, {-2,  0}, { 1,  0}, {-2, -1}, { 1,  2}},          // 3 -> 2
        }
    };
    static const SRSKickData::Kick O[4][2][1] = {{
            {{ 0,  0}},                                                  // 0 -> 1
            {{ 0,  0}}                                                   // 0 -> 3
        }, {
            {{ 0,  0}},                                                  // 1 -> 2
            {{ 0,  0}}                                                   // 1 -> 0
        }, {
            {{ 0,  0}},                                                  // 2 -> 3
            {{ 0,  0}}                                                   // 2 -> 1
        }, {
            {{ 0,  0}},                                                  // 3 -> 0
            {{ 0,  0}}                                                   // 3 -> 2
        }
    };
    static const SRSKickData::Kick IO180[4][1][1] = {{
            {{ 0,  0}},                                                  // 0 -> 2
        }, {
            {{ 0,  0}},                                                  // 1 -> 3
        }, {
            {{ 0,  0}},                                                  // 2 -> 0
        }, {
            {{ 0,  0}},                                                  // 3 -> 1
        }
    };
    switch (type) {
    case BlockType::J:
    case BlockType::L:
    case BlockType::S:
    case BlockType::T:
    case BlockType::Z:
        if (rot == Rotation::CW || rot == Rotation::CCW) {
            return {
                .kicks  = JLSTZ[orientation][uint8_t(rot)],
                .length = 5
            };
        } else if (rot == Rotation::HALF) {
            return {
                .kicks  = JLSTZ180[orientation][0],
                .length = 6
            };
        }
        break;
    case BlockType::I:
        if (rot == Rotation::CW || rot == Rotation::CCW) {
            return {
                .kicks  = I[orientation][uint8_t(rot)],
                .length = 5
            };
        } else if (rot == Rotation::HALF) {
            return {
                .kicks  = IO180[orientation][0],
                .length = 1
            };
        }
        break;
    case BlockType::O:
        if (rot == Rotation::CW || rot == Rotation::CCW) {
            return {
                .kicks  = O[orientation][uint8_t(rot)],
                .length = 1
            };
        } else if (rot == Rotation::HALF) {
            return {
                .kicks  = IO180[orientation][0],
                .length = 1
            };
        }
        break;
    default:
        break;
    }
    return {
        .kicks  = nullptr,
        .length = 0
    };
}

// srs_table[<block-type>][<block-orientation>][<rotate-direction>].kicks[<srs-index>]
const SRSKickData srs_table[int8_t(BlockType::SIZE)][4][uint8_t(Rotation::SIZE)] = {
    {
        {getSRSKickData(BlockType::Z, 0, Rotation::CW), getSRSKickData(BlockType::Z, 0, Rotation::CCW), getSRSKickData(BlockType::Z, 0, Rotation::HALF)},
        {getSRSKickData(BlockType::Z, 1, Rotation::CW), getSRSKickData(BlockType::Z, 1, Rotation::CCW), getSRSKickData(BlockType::Z, 1, Rotation::HALF)},
        {getSRSKickData(BlockType::Z, 2, Rotation::CW), getSRSKickData(BlockType::Z, 2, Rotation::CCW), getSRSKickData(BlockType::Z, 2, Rotation::HALF)},
        {getSRSKickData(BlockType::Z, 3, Rotation::CW), getSRSKickData(BlockType::Z, 3, Rotation::CCW), getSRSKickData(BlockType::Z, 3, Rotation::HALF)}
    },{
        {getSRSKickData(BlockType::L, 0, Rotation::CW), getSRSKickData(BlockType::L, 0, Rotation::CCW), getSRSKickData(BlockType::L, 0, Rotation::HALF)},
        {getSRSKickData(BlockType::L, 1, Rotation::CW), getSRSKickData(BlockType::L, 1, Rotation::CCW), getSRSKickData(BlockType::L, 1, Rotation::HALF)},
        {getSRSKickData(BlockType::L, 2, Rotation::CW), getSRSKickData(BlockType::L, 2, Rotation::CCW), getSRSKickData(BlockType::L, 2, Rotation::HALF)},
        {getSRSKickData(BlockType::L, 3, Rotation::CW), getSRSKickData(BlockType::L, 3, Rotation::CCW), getSRSKickData(BlockType::L, 3, Rotation::HALF)}
    },{
        {getSRSKickData(BlockType::O, 0, Rotation::CW), getSRSKickData(BlockType::O, 0, Rotation::CCW), getSRSKickData(BlockType::O, 0, Rotation::HALF)},
        {getSRSKickData(BlockType::O, 1, Rotation::CW), getSRSKickData(BlockType::O, 1, Rotation::CCW), getSRSKickData(BlockType::O, 1, Rotation::HALF)},
        {getSRSKickData(BlockType::O, 2, Rotation::CW), getSRSKickData(BlockType::O, 2, Rotation::CCW), getSRSKickData(BlockType::O, 2, Rotation::HALF)},
        {getSRSKickData(BlockType::O, 3, Rotation::CW), getSRSKickData(BlockType::O, 3, Rotation::CCW), getSRSKickData(BlockType::O, 3, Rotation::HALF)}
    },{
        {getSRSKickData(BlockType::S, 0, Rotation::CW), getSRSKickData(BlockType::S, 0, Rotation::CCW), getSRSKickData(BlockType::S, 0, Rotation::HALF)},
        {getSRSKickData(BlockType::S, 1, Rotation::CW), getSRSKickData(BlockType::S, 1, Rotation::CCW), getSRSKickData(BlockType::S, 1, Rotation::HALF)},
        {getSRSKickData(BlockType::S, 2, Rotation::CW), getSRSKickData(BlockType::S, 2, Rotation::CCW), getSRSKickData(BlockType::S, 2, Rotation::HALF)},
        {getSRSKickData(BlockType::S, 3, Rotation::CW), getSRSKickData(BlockType::S, 3, Rotation::CCW), getSRSKickData(BlockType::S, 3, Rotation::HALF)}
    },{
        {getSRSKickData(BlockType::I, 0, Rotation::CW), getSRSKickData(BlockType::I, 0, Rotation::CCW), getSRSKickData(BlockType::I, 0, Rotation::HALF)},
        {getSRSKickData(BlockType::I, 1, Rotation::CW), getSRSKickData(BlockType::I, 1, Rotation::CCW), getSRSKickData(BlockType::I, 1, Rotation::HALF)},
        {getSRSKickData(BlockType::I, 2, Rotation::CW), getSRSKickData(BlockType::I, 2, Rotation::CCW), getSRSKickData(BlockType::I, 2, Rotation::HALF)},
        {getSRSKickData(BlockType::I, 3, Rotation::CW), getSRSKickData(BlockType::I, 3, Rotation::CCW), getSRSKickData(BlockType::I, 3, Rotation::HALF)}
    },{
        {getSRSKickData(BlockType::J, 0, Rotation::CW), getSRSKickData(BlockType::J, 0, Rotation::CCW), getSRSKickData(BlockType::J, 0, Rotation::HALF)},
        {getSRSKickData(BlockType::J, 1, Rotation::CW), getSRSKickData(BlockType::J, 1, Rotation::CCW), getSRSKickData(BlockType::J, 1, Rotation::HALF)},
        {getSRSKickData(BlockType::J, 2, Rotation::CW), getSRSKickData(BlockType::J, 2, Rotation::CCW), getSRSKickData(BlockType::J, 2, Rotation::HALF)},
        {getSRSKickData(BlockType::J, 3, Rotation::CW), getSRSKickData(BlockType::J, 3, Rotation::CCW), getSRSKickData(BlockType::J, 3, Rotation::HALF)}
    },{
        {getSRSKickData(BlockType::T, 0, Rotation::CW), getSRSKickData(BlockType::T, 0, Rotation::CCW), getSRSKickData(BlockType::T, 0, Rotation::HALF)},
        {getSRSKickData(BlockType::T, 1, Rotation::CW), getSRSKickData(BlockType::T, 1, Rotation::CCW), getSRSKickData(BlockType::T, 1, Rotation::HALF)},
        {getSRSKickData(BlockType::T, 2, Rotation::CW), getSRSKickData(BlockType::T, 2, Rotation::CCW), getSRSKickData(BlockType::T, 2, Rotation::HALF)},
        {getSRSKickData(BlockType::T, 3, Rotation::CW), getSRSKickData(BlockType::T, 3, Rotation::CCW), getSRSKickData(BlockType::T, 3, Rotation::HALF)}
    },
};

inline static uint32_t xorshf32(uint32_t& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

struct SevenBagPermutations {
    struct PackedSevenBag {
        unsigned b0 : 3;
        unsigned b1 : 3;
        unsigned b2 : 3;
        unsigned b3 : 3;
        unsigned b4 : 3;
        unsigned b5 : 3;
        unsigned b6 : 3;
    };
    // 7! = 5040
    enum { SIZE = 5040 };
    PackedSevenBag data[SIZE];
};
inline static SevenBagPermutations generateSevenBagPermutations() {
    SevenBagPermutations permutations;
    BlockType next[] = { BlockType::Z, BlockType::L, BlockType::O, BlockType::S, BlockType::I, BlockType::J, BlockType::T };
    int index = 0;
    do {
        auto& ref = permutations.data[index++];
        ref.b0 = unsigned(next[0]);
        ref.b1 = unsigned(next[1]);
        ref.b2 = unsigned(next[2]);
        ref.b3 = unsigned(next[3]);
        ref.b4 = unsigned(next[4]);
        ref.b5 = unsigned(next[5]);
        ref.b6 = unsigned(next[6]);
    } while (std::next_permutation(next, next + 7));
    assert(index == SevenBagPermutations::SIZE);
    return permutations;
}

inline static void randomBlocks(BlockType dest[], uint32_t& seed) {
    static const SevenBagPermutations seven_bag_permutations = generateSevenBagPermutations();
    auto& ref = seven_bag_permutations.data[xorshf32(seed) % SevenBagPermutations::SIZE];
    dest[0] = BlockType(ref.b0);
    dest[1] = BlockType(ref.b1);
    dest[2] = BlockType(ref.b2);
    dest[3] = BlockType(ref.b3);
    dest[4] = BlockType(ref.b4);
    dest[5] = BlockType(ref.b5);
    dest[6] = BlockType(ref.b6);
}

inline static void clean(State* state) {
    using Initializer = IndexGenerator<Wrapper, BOARD_HEIGHT>::result::BoardInitializer<BOARD_HEIGHT, BOARD_PADDING,
        mkrow("BBB          BBB"),  // row data
        mkrow("BBBBBBBBBBBBBBBB")>; // wall data
    state->board = Initializer::board;
}

// returns {is_tspin, is_mini_tspin}
inline static std::pair<bool, bool> isTspin(State* state) {
    if (state->current != BlockType::T || !state->was_last_rotation) { return {false, false}; }
    // check corners around the T block
    // |0# | #1| # | # |
    // |###|###|###|###|
    // |   |   |2  |  3|
    bool corners[4] = {
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("B  ")}, state->x, state->y + BOARD_TOP),
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("  B")}, state->x, state->y + BOARD_TOP),
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("B  ")}, state->x, state->y + BOARD_TOP + 2),
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("  B")}, state->x, state->y + BOARD_TOP + 2),
    };
    int count = 0;
    for (int i = 0; i < 4; ++i) { count += corners[i]; }
    if (count < 3) { return {false, false}; }
    // check for mini tspin (https://tetris.wiki/T-Spin)
    // orientation -> two corner indices
    constexpr std::pair<uint32_t, uint32_t> mini_check_corner_table[] = {
        {0, 1},
        {1, 3},
        {2, 3},
        {0, 2},
    };
    bool is_mini
        = state->srs_index < 4 // not mini if used 5th kick
        && (corners[mini_check_corner_table[state->orientation].first] == 0
        || corners[mini_check_corner_table[state->orientation].second] == 0);
    return {true, is_mini};
}
inline static bool isAllSpin(State* state) {
    if (state->current == BlockType::T || !state->was_last_rotation) { return false; }
    auto& block = ops::getBlock(state->current, state->orientation);
    ops::removeBlock(state->board, block, state->x, state->y + BOARD_TOP);
    // check all-spin (piece cannot move left, right, up or down)
    bool collisions[4] = {
        !ops::canPlaceBlock(state->board, block, state->x - 1, state->y + BOARD_TOP),     // left
        !ops::canPlaceBlock(state->board, block, state->x + 1, state->y + BOARD_TOP),     // right
        !ops::canPlaceBlock(state->board, block, state->x,     state->y + BOARD_TOP - 1), // up
        !ops::canPlaceBlock(state->board, block, state->x,     state->y + BOARD_TOP + 1), // down
    };
    ops::placeBlock(state->board, block, state->x, state->y + BOARD_TOP);
    return collisions[0] && collisions[1] && collisions[2] && collisions[3];
}
inline static SpinType getSpinType(State* state) {
    if (state->current == BlockType::T) {
        auto [is_tspin, is_mini] = isTspin(state);
        if (is_tspin) { return is_mini ? SpinType::SPIN_MINI : SpinType::SPIN; }
    } else if (isAllSpin(state)) {
        return SpinType::SPIN_MINI;
    }
    return SpinType::NONE;
}
inline static int clearLines(State* state) {
    constexpr uint32_t fullfilled = mkrow("...GGGGGGGGGG...");
    int count = 0;
    for (int i = BOARD_BOTTOM; i >= BOARD_TOP - 3 /* TODO: Fix this */; i--) {
        while ((state->board.data[i - count] & fullfilled) == fullfilled) {
            state->board.data[i - count] = mkrow("BBB..........BBB");
            count++;
        }
        state->board.data[i] = state->board.data[i - count];
    }
    return count;
}

inline static bool isBackToBackSpinType(BlockType block_type, SpinType spin_type) {
    // current ruleset: tspin only
    if (block_type != BlockType::T) { return false; }
    if (spin_type == SpinType::SPIN || spin_type == SpinType::SPIN_MINI) { return true; }
    return false;
}

inline static bool moveBlock(State* state, int new_x, int new_y) {
    auto& block = ops::getBlock(state->current, state->orientation);
    ops::removeBlock(state->board, block, state->x, state->y + BOARD_TOP);
    bool can_place = ops::canPlaceBlock(state->board, block, new_x, new_y + BOARD_TOP);
    if (can_place) {
        state->x = new_x;
        state->y = new_y;
    }
    ops::placeBlock(state->board, block, state->x, state->y + BOARD_TOP);
    return can_place;
}
inline static bool rotateBlock(State* state, Rotation rot) {
    constexpr int8_t orientation_delta_table[uint8_t(Rotation::SIZE)] = {
        1, // CW  -> orientation + 1
        3, // CCW -> orientation - 1 (= +3 mod 4)
        2, // 180 -> orientation + 2
    };
    const int8_t new_orientation = (state->orientation + orientation_delta_table[uint8_t(rot)]) % 4;
    auto& old_block = ops::getBlock(state->current, state->orientation);
    auto& new_block = ops::getBlock(state->current, new_orientation);
    ops::removeBlock(state->board, old_block, state->x, state->y + BOARD_TOP);
    // SRS kicks for CW/CCW/180
    auto& [kicks, len] = srs_table[int8_t(state->current)][state->orientation][uint8_t(rot)];
    // try SRS kicks
    for (int i = 0; i < len; ++i) {
        const int test_x = state->x + kicks[i].x;
        const int test_y = state->y - kicks[i].y;
        if (ops::canPlaceBlock(state->board, new_block, test_x, test_y + BOARD_TOP)) {
            // commit rotation + kick
            state->orientation = new_orientation;
            state->x = test_x;
            state->y = test_y;
            ops::placeBlock(state->board, new_block, state->x, state->y + BOARD_TOP);
            state->srs_index = i;
            return true;
        }
    }
    ops::placeBlock(state->board, old_block, state->x, state->y + BOARD_TOP);
    return false;
}

void setSeed(State* state, uint32_t seed) {
    state->seed = seed;
}

void reset(State* state) {
    clean(state);
    state->is_alive = true;
    randomBlocks(state->next, state->seed);
    randomBlocks(state->next + 7, state->seed);
    state->hold = BlockType::NONE;
    state->has_held = false;
    state->current = BlockType::NONE;
    state->orientation = 0;
    state->x = -1;
    state->y = -1;
    state->lines_cleared = 0;
    state->srs_index = -1;
    state->piece_count = -1;
    state->was_last_rotation = false;
    // -1 because the first clear is not considered a back-to-back or a combo
    state->back_to_back_count = -1;
    state->combo_count = -1;
}
bool generateBlock(State* state, bool called_by_hold) {
    if (!called_by_hold) {
        int line_count = clearLines(state);
        state->lines_cleared += line_count;
        state->has_held = false;
        state->piece_count++;
        if (line_count > 0) {
            state->combo_count++;
            state->back_to_back_count
                = (isBackToBackSpinType(state->current, state->spin_type) || line_count == 4) // T-spin or Tetris
                ? state->back_to_back_count + 1
                : -1;
        } else {
            state->combo_count = -1;
        }
    }
    // reset state for new block
    state->srs_index = -1;
    state->was_last_rotation = false;
    // state->spin_type = SpinType::NONE; // postpone for tracking last spin type
    // set new current block
    state->current = state->next[0];
    state->x = BOARD_LEFT + 3;
    state->y = 0;
    state->orientation = 0;
    // shift the next blocks
    for (int i = 0; i < 13; i++) { state->next[i] = state->next[i + 1]; }
    state->next[13] = BlockType::NONE;
    // generate new random blocks if needed
    if (state->next[7] == BlockType::NONE) { randomBlocks(state->next + 7, state->seed); }
    // spawn the new block
    auto& block = ops::getBlock(state->current, state->orientation);
    bool can_place = ops::canPlaceBlock(state->board, block, state->x, state->y + BOARD_TOP);
    // Top out rule (https://tetris.wiki/Top_out)
    if (!can_place) {
        state->is_alive = false;
        return false;
    }
    ops::placeBlock(state->board, block, state->x, state->y + BOARD_TOP);
    return true;
}

bool moveLeft(State* state) {
    bool moved = moveBlock(state, state->x - 1, state->y);
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool moveRight(State* state) {
    bool moved = moveBlock(state, state->x + 1, state->y);
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool moveLeftToWall(State* state) {
    bool moved = false;
    while (moveBlock(state, state->x - 1, state->y)) { moved = true; }
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool moveRightToWall(State* state) {
    bool moved = false;
    while (moveBlock(state, state->x + 1, state->y)) { moved = true; }
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool softDrop(State* state) {
    bool moved = moveBlock(state, state->x, state->y + 1);
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool softDropToFloor(State* state) {
    bool moved = false;
    while (moveBlock(state, state->x, state->y + 1)) { moved = true; }
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool hardDrop(State* state) {
    bool moved = false;
    while (moveBlock(state, state->x, state->y + 1)) { moved = true; }
    if (moved) { state->was_last_rotation = false; }
    state->spin_type = getSpinType(state); // TODO: remove redundant check
    return generateBlock(state, false);
}
bool rotateCounterclockwise(State* state) {
    bool moved = rotateBlock(state, Rotation::CCW);
    if (moved) {
        state->was_last_rotation = true;
        state->spin_type = getSpinType(state);
    }
    return moved;
}
bool rotateClockwise(State* state) {
    bool moved = rotateBlock(state, Rotation::CW);
    if (moved) {
        state->was_last_rotation = true;
        state->spin_type = getSpinType(state);
    }
    return moved;
}
bool rotate180(State* state) {
    bool moved = rotateBlock(state, Rotation::HALF);
    if (moved) {
        state->was_last_rotation = true;
        state->spin_type = getSpinType(state);
    }
    return moved;
}
bool hold(State* state) {
    if (state->has_held) { return false; }
    // reset state for new block
    state->was_last_rotation = false;
    state->spin_type = SpinType::NONE;
    // push the held block to the block queue if exists
    if (state->hold != BlockType::NONE) {
        for (int i = 13; i > 0; i--) { state->next[i] = state->next[i - 1]; }
        state->next[0] = state->hold;
    }
    state->hold = state->current;
    // place new block
    ops::removeBlock(state->board, ops::getBlock(state->current, state->orientation), state->x, state->y + BOARD_TOP);
    generateBlock(state, true);
    state->has_held = true;
    return true;
}

void toString(State* state, char* buf, size_t size) {
    struct StringLayout {
        char board[22][25];
        // "Next: [Z, L, O, S, I, J, T, Z, L, O, S, I, J, T]"
        char next[49];
        // "Hold: [Z]"
        char hold[10];
        // "Line: [    % 8d]"
        char line[17];
    };
    static const char board[22][25] = {
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','X','X','\n'},
        {'X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','X','\n'},
    };
    static const char next[] = {
        'N','e','x','t',':',' ','[',
        '?',',',' ','?',',',' ','?',',',' ','?',',',' ','?',',',' ','?',',',' ','?',',',' ',
        '?',',',' ','?',',',' ','?',',',' ','?',',',' ','?',',',' ','?',',',' ','?',']','\n'
    };
    static const char hold[] = {'H','o','l','d',':',' ','[','Z',']','\n'};
    thread_local static State st;
    if (size < sizeof(StringLayout)) return;
    // get shadow of current block
    st.board = state->board;
    st.current = state->current;
    st.orientation = state->orientation;
    st.x = state->x;
    st.y = state->y;
    while (softDrop(&st));
    // print state to buf
    StringLayout* sl = reinterpret_cast<StringLayout*>(buf);
    memcpy(sl->board, board, sizeof(sl->board));
    memcpy(sl->next, next, sizeof(sl->next));
    memcpy(sl->hold, hold, sizeof(sl->hold));
    for (int i = 0; i < 21; i++) {
        uint32_t row = state->board.data[i + BOARD_TOP];
        uint32_t shd_row = st.board.data[i + BOARD_TOP];
        for (uint32_t mask = 0b00000000000000000000000011000000u, j = 21;
                      mask < 0b00000100000000000000000000100000u; mask <<= 2, j -= 2) {
            switch ((shd_row & mask) >> (27 - j)) {
            case 0b10:
            case 0b11:
                sl->board[i][j] = ':';
                sl->board[i][j - 1] = ':';
            }
            switch ((row & mask) >> (27 - j)) {
            case 0b01:
                sl->board[i][j] = sl->board[i][j - 1] = '#';
                break;
            case 0b10:
            case 0b11:
                sl->board[i][j] = ']';
                sl->board[i][j - 1] = '[';
                break;
            }
        }
    }
    for (int i = 0; i < 14; i++) { sl->next[7 + i * 3] = int8_t(state->next[i])["?ZLOSIJT" + 1]; }
    sl->hold[7] = int8_t(state->hold)[" ZLOSIJT" + 1];
    sprintf(sl->line, "Line: [% 8d]", state->lines_cleared);
    buf[sizeof(StringLayout) - 1] = '\0';
}

void eraseCurrent(State* state) {
    ops::removeBlock(state->board, ops::getBlock(state->current, state->orientation), state->x, state->y + BOARD_TOP);
}
bool pasteCurrent(State* state) {
    auto& block = ops::getBlock(state->current, state->orientation);
    if (!ops::canPlaceBlock(state->board, block, state->x, state->y + BOARD_TOP)) { return false; }
    ops::placeBlock(state->board, block, state->x, state->y + BOARD_TOP);
    return true;
}
