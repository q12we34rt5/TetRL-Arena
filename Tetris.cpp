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

inline static void initializeBoard(Board& board) {
    using Initializer = IndexGenerator<Wrapper, BOARD_HEIGHT>::result::BoardInitializer<BOARD_HEIGHT, BOARD_PADDING,
        mkrow("BBB          BBB"),  // row data
        mkrow("BBBBBBBBBBBBBBBB")>; // wall data
    board = Initializer::board;
}

// returns {is_tspin, is_mini_tspin}
inline static std::pair<bool, bool> isTspin(State* state) {
    if (state->current != BlockType::T || !state->was_last_rotation) { return {false, false}; }
    // check corners around the T block
    // |0# | #1| # | # |
    // |###|###|###|###|
    // |   |   |2  |  3|
    bool corners[4] = {
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("B  ")}, state->x, state->y),
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("  B")}, state->x, state->y),
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("B  ")}, state->x, state->y + 2),
        !ops::canPlaceRows(state->board, Rows<1>{mkrow("  B")}, state->x, state->y + 2),
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
    ops::removeBlock(state->board, block, state->x, state->y);
    // check all-spin (piece cannot move left, right, up or down)
    bool collisions[4] = {
        !ops::canPlaceBlock(state->board, block, state->x - 1, state->y), // left
        !ops::canPlaceBlock(state->board, block, state->x + 1, state->y), // right
        !ops::canPlaceBlock(state->board, block, state->x, state->y - 1), // up
        !ops::canPlaceBlock(state->board, block, state->x, state->y + 1), // down
    };
    ops::placeBlock(state->board, block, state->x, state->y);
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
inline static bool isBackToBackSpinType(BlockType block_type, SpinType spin_type) {
    // current ruleset: tspin only
    if (block_type != BlockType::T) { return false; }
    if (spin_type == SpinType::SPIN || spin_type == SpinType::SPIN_MINI) { return true; }
    return false;
}

int calculateAttack(const State* state, int cleared_lines) {
    // TODO: implement different rulesets
    return 0;
}

inline static void applyGarbage(Board& board, int lines, int hole_position) {
    if (lines <= 0) { return; }
    constexpr uint32_t garbage = mkrow("BBBGGGGGGGGGGBBB");
    // shift up
    for (int i = 0; i + lines <= BOARD_BOTTOM; ++i) {
        board.data[i] = board.data[i + lines];
    }
    // add garbage rows
    const uint32_t row = garbage & ~ops::shift(uint32_t(Cell::BLOCK), hole_position);
    for (int i = 0; i < lines; ++i) {
        board.data[BOARD_BOTTOM - i] = row;
    }
}

inline static int processGarbageAndCounterAttack(State* state, int attack, int cleared_lines) {
    int garbage_begin = 0, garbage_end = 0; // [begin, end) of pending garbage segments
    // decrement delays and records the range of pending garbage
    for (int i = 0; i < GARBAGE_QUEUE_SIZE; ++i) {
        if (state->garbage_queue[i] > 0) {
            if (state->garbage_delay[i] > 0) { state->garbage_delay[i]--; }
            garbage_end = i + 1; // update end to include this entry
        } else {
            break; // stop at first empty entry
        }
    }
    // counter garbage with attack and update the range
    int remaining_attack = attack;
    for (int i = garbage_begin; remaining_attack > 0 && i < garbage_end; ++i) {
        assert(state->garbage_queue[i] > 0);
        if (state->garbage_queue[i] <= remaining_attack) { // fully countered
            remaining_attack -= state->garbage_queue[i];
            state->garbage_queue[i] = 0;
            state->garbage_delay[i] = 0;
            garbage_begin++; // track begin index
        } else { // partially countered
            state->garbage_queue[i] -= remaining_attack;
            remaining_attack = 0;
        }
    }
    // apply pending garbage with zero delay if no garbage blocking or no lines cleared
    if (!state->garbage_blocking || cleared_lines == 0) {
        uint16_t total_garbage_spawned = 0;
        for (int i = garbage_begin; i < garbage_end; ++i) {
            assert(state->garbage_queue[i] > 0);
            if (state->garbage_delay[i] > 0) {
                assert(garbage_begin == i);
                break; // stop at first delayed garbage (assume delays are in order)
            }
            uint8_t lines_to_spawn = state->garbage_queue[i];
            if (total_garbage_spawned + lines_to_spawn > state->max_garbage_spawn) {
                lines_to_spawn = state->max_garbage_spawn - total_garbage_spawned;
            }
            assert(lines_to_spawn > 0);
            total_garbage_spawned += lines_to_spawn;
            // generate random hole position (assuming 10-wide playfield)
            int hole_position = xorshf32(state->garbage_seed) % 10 + BOARD_LEFT;
            // apply this segment of garbage
            applyGarbage(state->board, lines_to_spawn, hole_position);
            // update remaining garbage in this entry
            state->garbage_queue[i] -= lines_to_spawn;
            if (state->garbage_queue[i] == 0) {
                state->garbage_delay[i] = 0;
                garbage_begin++; // track begin index
                assert(garbage_begin == i + 1);
            } else {
                assert(garbage_begin == i);
                break; // stop if this entry is not fully applied (only happens when max_garbage_spawn is reached)
            }
        }
    }
    // shift remaining garbage entries to the front of the queue [garbage_begin, garbage_end) -> [0, garbage_end - garbage_begin)
    if (garbage_begin > 0) {
        int write_index = 0;
        for (int read_index = garbage_begin; read_index < garbage_end; ++read_index) {
            assert(state->garbage_queue[read_index] > 0);
            if (write_index != read_index) {
                state->garbage_queue[write_index] = state->garbage_queue[read_index];
                state->garbage_delay[write_index] = state->garbage_delay[read_index];
            }
            write_index++;
        }
        // clear remaining entries after compacted data
        for (int i = write_index; i < garbage_end; ++i) {
            state->garbage_queue[i] = 0;
            state->garbage_delay[i] = 0;
        }
    }
    return remaining_attack;
}

inline static int clearLines(State* state) {
    constexpr uint32_t empty = mkrow("BBB..........BBB");
    constexpr uint32_t fullfilled = mkrow("...GGGGGGGGGG...");
    int count = 0;
    for (int i = BOARD_BOTTOM; i >= 0; i--) {
        while (i - count >= 0 && (state->board.data[i - count] & fullfilled) == fullfilled) {
            count++;
        }
        if (i - count >= 0) {
            state->board.data[i] = state->board.data[i - count];
        } else {
            state->board.data[i] = empty;
        }
    }
    return count;
}
inline static void processPiecePlacement(State* state) {
    // clear lines and update state
    int cleared_lines = clearLines(state);
    state->lines_cleared += cleared_lines;
    state->piece_count++;
    if (cleared_lines > 0) {
        state->combo_count++;
        state->back_to_back_count
            = (isBackToBackSpinType(state->current, state->spin_type) || cleared_lines == 4) // T-spin or Tetris
            ? state->back_to_back_count + 1
            : -1;
    } else {
        state->combo_count = -1;
    }
    // calculate attack and counter garbage
    int attack = calculateAttack(state, cleared_lines);
    int lines_sent = processGarbageAndCounterAttack(state, attack, cleared_lines);
    state->attack = attack;
    state->lines_sent = lines_sent;
    state->total_attack += attack;
    state->total_lines_sent += lines_sent;
}
inline static BlockType fetchNextBlock(State* state) {
    BlockType next_block = state->next[0];
    // shift the next blocks
    for (int i = 0; i < 13; i++) { state->next[i] = state->next[i + 1]; }
    state->next[13] = BlockType::NONE;
    // generate new random blocks if needed
    if (state->next[7] == BlockType::NONE) { randomBlocks(state->next + 7, state->seed); }
    return next_block;
}
inline static bool newCurrentBlock(State* state, BlockType block_type) {
    // set new current block
    state->current = block_type;
    state->orientation = 0;
    state->x = BOARD_LEFT + 3;
    state->y = BOARD_TOP;
    // reset state for new block
    state->srs_index = -1;
    state->was_last_rotation = false;
    // state->spin_type = SpinType::NONE; // postpone for tracking last spin type
    // spawn the new block
    auto& block = ops::getBlock(state->current, state->orientation);
    bool can_place = ops::canPlaceBlock(state->board, block, state->x, state->y);
    // Top out rule (https://tetris.wiki/Top_out)
    if (!can_place) {
        state->is_alive = false;
        return false;
    }
    ops::placeBlock(state->board, block, state->x, state->y);
    return true;
}

inline static bool moveBlock(State* state, int new_x, int new_y) {
    auto& block = ops::getBlock(state->current, state->orientation);
    ops::removeBlock(state->board, block, state->x, state->y);
    bool can_place = ops::canPlaceBlock(state->board, block, new_x, new_y);
    if (can_place) {
        state->x = new_x;
        state->y = new_y;
    }
    ops::placeBlock(state->board, block, state->x, state->y);
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
    ops::removeBlock(state->board, old_block, state->x, state->y);
    // SRS kicks for CW/CCW/180
    auto& [kicks, len] = srs_table[int8_t(state->current)][state->orientation][uint8_t(rot)];
    // try SRS kicks
    for (int i = 0; i < len; ++i) {
        const int test_x = state->x + kicks[i].x;
        const int test_y = state->y - kicks[i].y;
        if (ops::canPlaceBlock(state->board, new_block, test_x, test_y)) {
            // commit rotation + kick
            state->orientation = new_orientation;
            state->x = test_x;
            state->y = test_y;
            ops::placeBlock(state->board, new_block, state->x, state->y);
            state->srs_index = i;
            return true;
        }
    }
    ops::placeBlock(state->board, old_block, state->x, state->y);
    return false;
}

void setSeed(State* state, uint32_t seed) {
    state->seed = seed;
}

void reset(State* state) {
    initializeBoard(state->board);
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
    state->piece_count = 0;
    state->was_last_rotation = false;
    state->spin_type = SpinType::NONE;
    // -1 because the first clear is not considered a back-to-back or a combo
    state->back_to_back_count = -1;
    state->combo_count = -1;
    // initialize garbage queues
    std::fill(std::begin(state->garbage_queue), std::end(state->garbage_queue), 0);
    std::fill(std::begin(state->garbage_delay), std::end(state->garbage_delay), 0);
    state->attack = 0;
    state->lines_sent = 0;
    state->total_attack = 0;
    state->total_lines_sent = 0;
    // spawn current block
    BlockType next_block = fetchNextBlock(state);
    newCurrentBlock(state, next_block);
}

bool moveLeft(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = moveBlock(state, state->x - 1, state->y);
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool moveRight(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = moveBlock(state, state->x + 1, state->y);
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool moveLeftToWall(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = false;
    while (moveBlock(state, state->x - 1, state->y)) { moved = true; }
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool moveRightToWall(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = false;
    while (moveBlock(state, state->x + 1, state->y)) { moved = true; }
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool softDrop(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = moveBlock(state, state->x, state->y + 1);
    if (moved) {
        state->was_last_rotation = false;
        state->spin_type = SpinType::NONE;
    }
    return moved;
}
bool softDropToFloor(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
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
    processPiecePlacement(state);
    BlockType next_block = fetchNextBlock(state);
    if (newCurrentBlock(state, next_block)) {
        state->has_held = false;
        return true;
    }
    return false;
}
bool rotateCounterclockwise(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = rotateBlock(state, Rotation::CCW);
    if (moved) {
        state->was_last_rotation = true;
        state->spin_type = getSpinType(state);
    }
    return moved;
}
bool rotateClockwise(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = rotateBlock(state, Rotation::CW);
    if (moved) {
        state->was_last_rotation = true;
        state->spin_type = getSpinType(state);
    }
    return moved;
}
bool rotate180(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    bool moved = rotateBlock(state, Rotation::HALF);
    if (moved) {
        state->was_last_rotation = true;
        state->spin_type = getSpinType(state);
    }
    return moved;
}
bool hold(State* state) {
    state->attack = 0;
    state->lines_sent = 0;
    if (state->has_held) { return false; }
    // reset state for new block (TODO: remove redundancy with newCurrentBlock)
    state->was_last_rotation = false;
    state->spin_type = SpinType::NONE;
    // hold current block
    BlockType new_block = state->hold != BlockType::NONE ? state->hold : fetchNextBlock(state);
    state->hold = state->current;
    state->has_held = true;
    // place new block
    ops::removeBlock(state->board, ops::getBlock(state->current, state->orientation), state->x, state->y);
    newCurrentBlock(state, new_block);
    return true;
}

bool addGarbage(State* state, uint8_t lines, uint8_t delay) {
    if (lines == 0) { return false; }
    for (int i = 0; i < GARBAGE_QUEUE_SIZE; ++i) {
        if (state->garbage_queue[i] == 0) {
            assert(state->garbage_delay[i] == 0);
            state->garbage_queue[i] = lines;
            state->garbage_delay[i] = delay;
            return true;
        }
    }
    // TODO: garbage queue full, maybe append to the last entry?
    return false;
}

void toString(State* state, char* buf, size_t size) {
    // TODO:
    //   - refactor
    //   - add garbage queue info
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
    ops::removeBlock(state->board, ops::getBlock(state->current, state->orientation), state->x, state->y);
}
bool pasteCurrent(State* state) {
    auto& block = ops::getBlock(state->current, state->orientation);
    if (!ops::canPlaceBlock(state->board, block, state->x, state->y)) { return false; }
    ops::placeBlock(state->board, block, state->x, state->y);
    return true;
}
