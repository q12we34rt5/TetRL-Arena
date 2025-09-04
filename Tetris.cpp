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

        static const uint32_t board[height];
    };
};
template<int ...args>
template<int height, int padding, uint32_t row, uint32_t wall>
const uint32_t Wrapper<args...>::template BoardInitializer<height, padding, row, wall>::board[height] = {
    Wrapper<args...>::template BoardInitializer<height, padding, row, wall>::template If<(/*args < padding || */args >= height - padding), int>::value...
};

const Block blocks[Block::Type::SIZE][4] = {{
    {0b11110000000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00001100000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00000000000000000000000000000000u,
     0b11110000000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00110000000000000000000000000000u,
     0b11110000000000000000000000000000u,
     0b11000000000000000000000000000000u,
     0b00000000000000000000000000000000u}},
{   {0b00001100000000000000000000000000u,
     0b11111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00000000000000000000000000000000u,
     0b11111100000000000000000000000000u,
     0b11000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b11110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00000000000000000000000000000000u}},
{   {0b00111100000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00111100000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00111100000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00111100000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u}},
{   {0b00111100000000000000000000000000u,
     0b11110000000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00110000000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00001100000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00000000000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b11110000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b11000000000000000000000000000000u,
     0b11110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00000000000000000000000000000000u}},
{   {0b00000000000000000000000000000000u,
     0b11111111000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00001100000000000000000000000000u,
     0b00001100000000000000000000000000u,
     0b00001100000000000000000000000000u,
     0b00001100000000000000000000000000u},
    {0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b11111111000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00110000000000000000000000000000u}},
{   {0b11000000000000000000000000000000u,
     0b11111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00111100000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00000000000000000000000000000000u,
     0b11111100000000000000000000000000u,
     0b00001100000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b11110000000000000000000000000000u,
     0b00000000000000000000000000000000u}},
{   {0b00110000000000000000000000000000u,
     0b11111100000000000000000000000000u,
     0b00000000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00110000000000000000000000000000u,
     0b00111100000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00000000000000000000000000000000u,
     0b11111100000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00000000000000000000000000000000u},
    {0b00110000000000000000000000000000u,
     0b11110000000000000000000000000000u,
     0b00110000000000000000000000000000u,
     0b00000000000000000000000000000000u}}
};

inline static SRSKickData getSRSKickData(Block::Type type, int8_t rotation, RotationDirection direction) {
    // [<block-rotation>][<direction>][<srs-index>]
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
    case Block::Type::J:
    case Block::Type::L:
    case Block::Type::S:
    case Block::Type::T:
    case Block::Type::Z:
        if (direction == RotationDirection::CLOCKWISE || direction == RotationDirection::COUNTERCLOCKWISE) {
            return {
                .kicks  = JLSTZ[rotation][uint8_t(direction)],
                .length = 5
            };
        } else if (direction == RotationDirection::HALFTURN) {
            return {
                .kicks  = JLSTZ180[rotation][0],
                .length = 6
            };
        }
        break;
    case Block::Type::I:
        if (direction == RotationDirection::CLOCKWISE || direction == RotationDirection::COUNTERCLOCKWISE) {
            return {
                .kicks  = I[rotation][uint8_t(direction)],
                .length = 5
            };
        } else if (direction == RotationDirection::HALFTURN) {
            return {
                .kicks  = IO180[rotation][0],
                .length = 1
            };
        }
        break;
    case Block::Type::O:
        if (direction == RotationDirection::CLOCKWISE || direction == RotationDirection::COUNTERCLOCKWISE) {
            return {
                .kicks  = O[rotation][uint8_t(direction)],
                .length = 1
            };
        } else if (direction == RotationDirection::HALFTURN) {
            return {
                .kicks  = IO180[rotation][0],
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

// srs_table[<block-type>][<block-rotation>][<direction>].kicks[<srs-index>]
const SRSKickData srs_table[Block::Type::SIZE][4][uint8_t(RotationDirection::SIZE)] = {
    {
        {getSRSKickData(Block::Type::Z, 0, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::Z, 0, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::Z, 0, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::Z, 1, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::Z, 1, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::Z, 1, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::Z, 2, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::Z, 2, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::Z, 2, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::Z, 3, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::Z, 3, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::Z, 3, RotationDirection::HALFTURN)}
    },{
        {getSRSKickData(Block::Type::L, 0, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::L, 0, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::L, 0, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::L, 1, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::L, 1, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::L, 1, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::L, 2, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::L, 2, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::L, 2, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::L, 3, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::L, 3, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::L, 3, RotationDirection::HALFTURN)}
    },{
        {getSRSKickData(Block::Type::O, 0, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::O, 0, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::O, 0, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::O, 1, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::O, 1, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::O, 1, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::O, 2, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::O, 2, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::O, 2, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::O, 3, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::O, 3, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::O, 3, RotationDirection::HALFTURN)}
    },{
        {getSRSKickData(Block::Type::S, 0, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::S, 0, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::S, 0, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::S, 1, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::S, 1, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::S, 1, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::S, 2, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::S, 2, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::S, 2, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::S, 3, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::S, 3, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::S, 3, RotationDirection::HALFTURN)}
    },{
        {getSRSKickData(Block::Type::I, 0, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::I, 0, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::I, 0, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::I, 1, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::I, 1, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::I, 1, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::I, 2, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::I, 2, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::I, 2, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::I, 3, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::I, 3, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::I, 3, RotationDirection::HALFTURN)}
    },{
        {getSRSKickData(Block::Type::J, 0, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::J, 0, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::J, 0, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::J, 1, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::J, 1, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::J, 1, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::J, 2, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::J, 2, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::J, 2, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::J, 3, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::J, 3, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::J, 3, RotationDirection::HALFTURN)}
    },{
        {getSRSKickData(Block::Type::T, 0, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::T, 0, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::T, 0, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::T, 1, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::T, 1, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::T, 1, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::T, 2, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::T, 2, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::T, 2, RotationDirection::HALFTURN)},
        {getSRSKickData(Block::Type::T, 3, RotationDirection::CLOCKWISE), getSRSKickData(Block::Type::T, 3, RotationDirection::COUNTERCLOCKWISE), getSRSKickData(Block::Type::T, 3, RotationDirection::HALFTURN)}
    },
};

inline static uint32_t xorshf32(uint32_t& seed) {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

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
static PackedSevenBag seven_bag_permutations[5040];
inline static void initializeSevenBagPermutations() {
    Block::Type next[] = { Block::Z, Block::L, Block::O, Block::S, Block::I, Block::J, Block::T };
    int index = 0;
    do {
        PackedSevenBag& ref = seven_bag_permutations[index++];
        ref.b0 = unsigned(next[0]);
        ref.b1 = unsigned(next[1]);
        ref.b2 = unsigned(next[2]);
        ref.b3 = unsigned(next[3]);
        ref.b4 = unsigned(next[4]);
        ref.b5 = unsigned(next[5]);
        ref.b6 = unsigned(next[6]);
    } while (std::next_permutation(next, next + 7));
}

inline static void randomBlocks(Block::Type dest[], uint32_t& seed) {
    PackedSevenBag& ref = seven_bag_permutations[xorshf32(seed) % 5040];
    dest[0] = Block::Type(ref.b0);
    dest[1] = Block::Type(ref.b1);
    dest[2] = Block::Type(ref.b2);
    dest[3] = Block::Type(ref.b3);
    dest[4] = Block::Type(ref.b4);
    dest[5] = Block::Type(ref.b5);
    dest[6] = Block::Type(ref.b6);
}

inline static void clean(State* state) {
    using Initializer = IndexGenerator<Wrapper, BOARD_HEIGHT>::result::BoardInitializer<BOARD_HEIGHT, BOARD_PADDING,
        0b11111100000000000000000000111111u,  // row data
        0b11111111111111111111111111111111u>; // wall data
    memcpy(state->board, Initializer::board, sizeof(state->board));
}

// returns {is_tspin, is_mini_tspin}
inline static std::pair<bool, bool> isTspin(State* state) {
    if (state->current != Block::Type::T || !state->was_last_rotation) { return {false, false}; }
    constexpr uint32_t mask = 0b11000000000000000000000000000000u;
    const int x_offset = state->x << 1;
    const int y_offset = state->y + BOARD_TOP;
    uint32_t& row0 = state->board[y_offset + 0];
    uint32_t& row2 = state->board[y_offset + 2];
    bool corners[4] = {
        (row0 & mask >> x_offset) != 0,
        (row0 & mask >> x_offset + 4) != 0,
        (row2 & mask >> x_offset) != 0,
        (row2 & mask >> x_offset + 4) != 0
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
        = corners[mini_check_corner_table[state->orientation].first] == 0
        || corners[mini_check_corner_table[state->orientation].second] == 0;
    return {true, is_mini};
}
inline static ClearType getClearType(int lines_cleared, bool tspin, bool tspin_mini) {
    if (tspin_mini) {
        switch (lines_cleared) {
        case 0: return ClearType::MINI_TSPIN;
        case 1: return ClearType::MINI_TSPIN_SINGLE;
        case 2: return ClearType::MINI_TSPIN_DOUBLE;
        default: assert(false); return ClearType::NONE;
        }
    } else if (tspin) {
        switch (lines_cleared) {
        case 0: return ClearType::TSPIN;
        case 1: return ClearType::TSPIN_SINGLE;
        case 2: return ClearType::TSPIN_DOUBLE;
        case 3: return ClearType::TSPIN_TRIPLE;
        default: assert(false); return ClearType::NONE;
        }
    } else {
        switch (lines_cleared) {
        case 0: return ClearType::NONE;
        case 1: return ClearType::SINGLE;
        case 2: return ClearType::DOUBLE;
        case 3: return ClearType::TRIPLE;
        case 4: return ClearType::QUAD;
        default: assert(false); return ClearType::NONE;
        }
    }
}
inline static int clearLines(State* state) {
    static constexpr uint32_t fullfilled = 0b00000001010101010101010101000000u;
    int count = 0;
    for (int i = BOARD_BOTTOM; i >= BOARD_TOP - 3 /* TODO: Fix this */; i--) {
        while ((state->board[i - count] & fullfilled) == fullfilled) {
            state->board[i - count] = 0b11111100000000000000000000111111u;
            count++;
        }
        state->board[i] = state->board[i - count];
    }
    return count;
}

inline static bool isBackToBackClear(ClearType type) {
    switch (type) {
    case ClearType::QUAD:
    case ClearType::TSPIN_SINGLE:
    case ClearType::TSPIN_DOUBLE:
    case ClearType::TSPIN_TRIPLE:
    case ClearType::MINI_TSPIN_SINGLE:
    case ClearType::MINI_TSPIN_DOUBLE:
        return true;
    default:
        return false;
    }
}

inline static bool moveBlock(State* state, int new_x, int new_y) {
    const auto& block_data = blocks[state->current][state->orientation].data;
    const int old_x_offset = state->x << 1;
    const int old_y_offset = state->y + BOARD_TOP;
    const int new_x_offset = new_x << 1;
    const int new_y_offset = new_y + BOARD_TOP;
    uint32_t* old_rows = &state->board[old_y_offset];
    uint32_t* new_rows = &state->board[new_y_offset];

    // compute old and new block positions
    uint32_t old_mask[4];
    uint32_t new_mask[4];
    for (int i = 0; i < 4; ++i) {
        const uint32_t r = block_data[i];
        old_mask[i] = r >> old_x_offset;
        new_mask[i] = r >> new_x_offset;
    }

    // remove old block from board
    for (int i = 0; i < 4; ++i) { old_rows[i] &= ~old_mask[i]; }

    // test new block position
    bool collision = false;
    for (int r = 0; r < 4; ++r) { collision |= (new_mask[r] & new_rows[r]) != 0; }

    // place block on board
    if (collision) {
        for (int i = 0; i < 4; ++i) { old_rows[i] |= old_mask[i]; }
    } else {
        state->x = new_x;
        state->y = new_y;
        for (int i = 0; i < 4; ++i) { new_rows[i] |= new_mask[i]; }
    }

    return !collision;
}
inline static bool rotateBlock(State* state, RotationDirection dir) {
    constexpr int8_t orientation_delta_table[uint8_t(RotationDirection::SIZE)] = {
        1, // CW
        3, // CCW
        2, // 180
    };

    const int8_t new_orientation = (state->orientation + orientation_delta_table[uint8_t(dir)]) % 4;

    // SRS kicks for CW/CCW/180
    auto& kicks = srs_table[state->current][state->orientation][uint8_t(dir)].kicks;
    auto  len   = srs_table[state->current][state->orientation][uint8_t(dir)].length;

    // current orientation bitmasks at current (x,y)
    const auto& cur_data = blocks[state->current][state->orientation].data;
    const int x_offset = state->x << 1;
    const int y_offset = state->y + BOARD_TOP;
    uint32_t* rows = &state->board[y_offset];

    // compute old block positions
    uint32_t old_mask[4];
    for (int i = 0; i < 4; ++i) { old_mask[i] = cur_data[i] >> x_offset; }

    // erase current piece to avoid self-collision
    for (int i = 0; i < 4; ++i) { rows[i] &= ~old_mask[i]; }

    // rotated orientation masks source
    const auto& rot_data = blocks[state->current][new_orientation].data;

    // try SRS kicks
    for (int i = 0; i < len; ++i) {
        const int dx_bits = kicks[i].x << 1;
        const int test_x  = x_offset + dx_bits;
        const int test_y  = y_offset - kicks[i].y; // board rows pointer for candidate
        uint32_t* test_rows = &state->board[test_y];

        uint32_t new_mask[4];
        for (int r = 0; r < 4; ++r) { new_mask[r] = rot_data[r] >> test_x; }

        bool collision = false;
        for (int r = 0; r < 4; ++r) { collision |= (new_mask[r] & test_rows[r]) != 0; }

        if (!collision) {
            // commit rotation + kick
            state->orientation = new_orientation;
            state->x += kicks[i].x;
            state->y -= kicks[i].y;
            for (int r = 0; r < 4; ++r) { test_rows[r] |= new_mask[r]; }
            state->srs_index = i;
            return true;
        }
    }

    // restore original piece if all kicks fail
    for (int i = 0; i < 4; ++i) { rows[i] |= old_mask[i]; }
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
    state->hold = Block::NONE;
    state->has_held = false;
    state->current = Block::NONE;
    state->orientation = 0;
    state->x = -1;
    state->y = -1;
    state->lines_cleared = 0;
    state->srs_index = -1;
    state->piece_count = 0;
    state->was_last_rotation = false;
    // -1 because the first clear is not considered a back-to-back or a combo
    state->back_to_back_count = -1;
    state->combo_count = -1;
}
bool generateBlock(State* state, bool called_by_hold) {
    if (!called_by_hold) {
        auto [is_tspin, is_tspin_mini] = isTspin(state);
        int line_count = clearLines(state);
        state->lines_cleared += line_count;
        state->last_clear_type = getClearType(line_count, is_tspin, is_tspin_mini);
        state->has_held = false;
        state->piece_count++;
        if (line_count > 0) {
            state->combo_count++;
            state->back_to_back_count
                = isBackToBackClear(state->last_clear_type) ? state->back_to_back_count + 1 : -1;
        } else {
            state->combo_count = -1;
        }
    }

    state->srs_index = 0;
    state->was_last_rotation = false;

    Block::Type cur = state->next[0];
    state->x = BOARD_LEFT + 3;
    state->y = 0;
    state->orientation = 0;

    auto& block_data = blocks[cur][0].data;
    const int x_offset = state->x << 1;
    const int y_offset = state->y + BOARD_TOP;
    uint32_t* rows = &state->board[y_offset];

    // compute new block positions
    uint32_t new_mask[4];
    for (int i = 0; i < 4; ++i) { new_mask[i] = block_data[i] >> x_offset; }

    // test collision
    bool collision = false;
    for (int i = 0; i < 4; ++i) { collision |= (new_mask[i] & rows[i]) != 0; }

    // Top out rule (https://tetris.wiki/Top_out)
    if (collision) {
        state->is_alive = false;
        return false;
    }

    state->current = cur;

    // commit new block positions
    for (int i = 0; i < 4; ++i) { rows[i] |= new_mask[i]; }

    // shift the next blocks
    for (int i = 0; i < 13; i++) { state->next[i] = state->next[i + 1]; }
    state->next[13] = Block::NONE;

    // generate new random blocks if needed
    if (state->next[7] == Block::NONE) { randomBlocks(state->next + 7, state->seed); }

    return true;
}

bool moveLeft (State* state) {
    bool moved = moveBlock(state, state->x - 1, state->y);
    if (moved) { state->was_last_rotation = false; }
    return moved;
}
bool moveRight(State* state) {
    bool moved = moveBlock(state, state->x + 1, state->y);
    if (moved) { state->was_last_rotation = false; }
    return moved;
}
bool moveLeftToWall(State* state) {
    bool moved = false;
    while (moveLeft(state)) { moved = true; }
    if (moved) { state->was_last_rotation = false; }
    return moved;
}
bool moveRightToWall(State* state) {
    bool moved = false;
    while (moveRight(state)) { moved = true; }
    if (moved) { state->was_last_rotation = false; }
    return moved;
}
bool softDrop(State* state)  {
    bool moved = moveBlock(state, state->x, state->y + 1);
    if (moved) { state->was_last_rotation = false; }
    return moved;
}
bool softDropToFloor(State* state) {
    bool moved = false;
    while (softDrop(state)) { moved = true; }
    if (moved) { state->was_last_rotation = false; }
    return moved;
}
bool hardDrop(State* state) {
    bool was_last_rotation = state->was_last_rotation;
    while (softDrop(state));
    state->was_last_rotation = was_last_rotation;
    return generateBlock(state, false);
}
bool rotateCounterclockwise(State* state) {
    bool moved = rotateBlock(state, RotationDirection::COUNTERCLOCKWISE);
    if (moved) { state->was_last_rotation = true; }
    return moved;
}
bool rotateClockwise(State* state)        {
    bool moved = rotateBlock(state, RotationDirection::CLOCKWISE);
    if (moved) { state->was_last_rotation = true; }
    return moved;
}
bool rotate180(State* state)              {
    bool moved = rotateBlock(state, RotationDirection::HALFTURN);
    if (moved) { state->was_last_rotation = true; }
    return moved;
}
bool hold(State* state) {
    if (state->has_held) { return false; }

    state->was_last_rotation = false;

    if (state->hold != Block::NONE) {
        // push the held block to the block queue
        for (int i = 13; i > 0; i--) { state->next[i] = state->next[i - 1]; }
        state->next[0] = state->hold;
    }
    state->hold = state->current;

    auto& block_data = blocks[state->current][state->orientation].data;
    const int x_offset = state->x << 1;
    const int y_offset = state->y + BOARD_TOP;
    uint32_t* rows = &state->board[y_offset];

    // compute block positions
    uint32_t block_mask[4];
    for (int i = 0; i < 4; ++i) { block_mask[i] = block_data[i] >> x_offset; }

    // remove block from board
    for (int i = 0; i < 4; ++i) { rows[i] &= ~block_mask[i]; }

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
    memcpy(st.board, state->board, sizeof(st.board));
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
        uint32_t row = state->board[i + BOARD_TOP];
        uint32_t shd_row = st.board[i + BOARD_TOP];
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
    for (int i = 0; i < 14; i++) { sl->next[7 + i * 3] = state->next[i]["?ZLOSIJT" + 1]; }
    sl->hold[7] = state->hold[" ZLOSIJT" + 1];
    sprintf(sl->line, "Line: [% 8d]", state->lines_cleared);
    buf[sizeof(StringLayout) - 1] = '\0';
}

void eraseCurrent(State* state) {
    auto& block_data = blocks[state->current][state->orientation].data;
    const int x_offset = state->x << 1;
    const int y_offset = state->y + BOARD_TOP;
    uint32_t* rows = &state->board[y_offset];

    // compute block positions
    uint32_t block_mask[4];
    for (int i = 0; i < 4; ++i) { block_mask[i] = block_data[i] >> x_offset; }

    // remove block from board
    for (int i = 0; i < 4; ++i) { rows[i] &= ~block_mask[i]; }
}
bool pasteCurrent(State* state) {
    auto& block_data = blocks[state->current][state->orientation].data;
    const int x_offset = state->x << 1;
    const int y_offset = state->y + BOARD_TOP;
    uint32_t* rows = &state->board[y_offset];

    // compute block positions
    uint32_t block_mask[4];
    for (int i = 0; i < 4; ++i) { block_mask[i] = block_data[i] >> x_offset; }

    // test collision
    for (int i = 0; i < 4; ++i) {
        if (block_mask[i] & rows[i]) { return false; }
    }

    // place block on board
    for (int i = 0; i < 4; ++i) { rows[i] |= block_mask[i]; }

    return true;
}

static int initialize() {
    initializeSevenBagPermutations();
    return 0;
}

static int _ = initialize();
