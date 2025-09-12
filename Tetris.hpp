#pragma once
#include <cstdint>
#include <cstddef>
#include <type_traits>

constexpr int BOARD_HEIGHT  = 32;
constexpr int BOARD_PADDING = 4;
constexpr int BOARD_TOP     = BOARD_HEIGHT - BOARD_PADDING - 21;
constexpr int BOARD_BOTTOM  = BOARD_TOP + 20;
constexpr int BOARD_LEFT    = 3;

enum class Cell : uint32_t {
    EMPTY   = 0b00'000000000000000000000000000000u,
    GARBAGE = 0b01'000000000000000000000000000000u,
    SHADOW  = 0b10'000000000000000000000000000000u,
    BLOCK   = 0b11'000000000000000000000000000000u,
};

template <std::size_t N>
struct Rows {
    enum { SIZE = N };
    uint32_t data[N];
};

using Board = Rows<BOARD_HEIGHT>;
using Block = Rows<4>;

/** 
 * Creates a 32-bit row representation from a C-style null-terminated string.
 * The string must be at most 16 characters long (excluding '\0').
 * Each character represents a cell: 'G' (GARBAGE), 'S' (SHADOW), 'B' (BLOCK),
 * or any other character (treated as EMPTY).
 * The input string is aligned to the most significant bits of the 32-bit row.
 * If the string is shorter than 16 characters, remaining bits are treated as EMPTY.
 */
template<int N, typename = std::enable_if_t<(N <= 17)>>
constexpr uint32_t mkrow(const char (&s)[N]) {
    uint32_t bits = 0;
    for (int i = 0; s[i]; ++i) {
        switch (s[i]) {
        case 'G': bits |= (static_cast<uint32_t>(Cell::GARBAGE) >> (i << 1)); break;
        case 'S': bits |= (static_cast<uint32_t>(Cell::SHADOW)  >> (i << 1)); break;
        case 'B': bits |= (static_cast<uint32_t>(Cell::BLOCK)   >> (i << 1)); break;
        default:  bits |= (static_cast<uint32_t>(Cell::EMPTY)   >> (i << 1)); break;
        }
    }
    return bits;
}

enum class BlockType : int8_t {
    NONE = -1,
    Z = 0, L, O, S, I, J, T,
    SIZE
};

constexpr Block blocks[int8_t(BlockType::SIZE)][4] = {{
    {mkrow("BB  "),
     mkrow(" BB "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow("  B "),
     mkrow(" BB "),
     mkrow(" B  "),
     mkrow("    ")},
    {mkrow("    "),
     mkrow("BB  "),
     mkrow(" BB "),
     mkrow("    ")},
    {mkrow(" B  "),
     mkrow("BB  "),
     mkrow("B   "),
     mkrow("    ")}},
{   {mkrow("  B "),
     mkrow("BBB "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow(" B  "),
     mkrow(" B  "),
     mkrow(" BB "),
     mkrow("    ")},
    {mkrow("    "),
     mkrow("BBB "),
     mkrow("B   "),
     mkrow("    ")},
    {mkrow("BB  "),
     mkrow(" B  "),
     mkrow(" B  "),
     mkrow("    ")}},
{   {mkrow(" BB "),
     mkrow(" BB "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow(" BB "),
     mkrow(" BB "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow(" BB "),
     mkrow(" BB "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow(" BB "),
     mkrow(" BB "),
     mkrow("    "),
     mkrow("    ")}},
{   {mkrow(" BB "),
     mkrow("BB  "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow(" B  "),
     mkrow(" BB "),
     mkrow("  B "),
     mkrow("    ")},
    {mkrow("    "),
     mkrow(" BB "),
     mkrow("BB  "),
     mkrow("    ")},
    {mkrow("B   "),
     mkrow("BB  "),
     mkrow(" B  "),
     mkrow("    ")}},
{   {mkrow("    "),
     mkrow("BBBB"),
     mkrow("    "),
     mkrow("    ")},
    {mkrow("  B "),
     mkrow("  B "),
     mkrow("  B "),
     mkrow("  B ")},
    {mkrow("    "),
     mkrow("    "),
     mkrow("BBBB"),
     mkrow("    ")},
    {mkrow(" B  "),
     mkrow(" B  "),
     mkrow(" B  "),
     mkrow(" B  ")}},
{   {mkrow("B   "),
     mkrow("BBB "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow(" BB "),
     mkrow(" B  "),
     mkrow(" B  "),
     mkrow("    ")},
    {mkrow("    "),
     mkrow("BBB "),
     mkrow("  B "),
     mkrow("    ")},
    {mkrow(" B  "),
     mkrow(" B  "),
     mkrow("BB  "),
     mkrow("    ")}},
{   {mkrow(" B  "),
     mkrow("BBB "),
     mkrow("    "),
     mkrow("    ")},
    {mkrow(" B  "),
     mkrow(" BB "),
     mkrow(" B  "),
     mkrow("    ")},
    {mkrow("    "),
     mkrow("BBB "),
     mkrow(" B  "),
     mkrow("    ")},
    {mkrow(" B  "),
     mkrow("BB  "),
     mkrow(" B  "),
     mkrow("    ")}}
};

enum class SpinType : uint8_t {
    NONE,
    SPIN,
    SPIN_MINI
};

struct State {
    Board board;
    bool is_alive;
    BlockType next[14];
    BlockType hold;
    bool has_held;
    BlockType current;
    int8_t orientation;
    int x, y;
    int lines_cleared;
    uint32_t seed;
    int srs_index;
    uint32_t piece_count;
    // TODO: remove was_last_rotation and use spin_type only
    bool was_last_rotation; // Indicates if the last successful action was a rotation
    SpinType spin_type;
    int32_t back_to_back_count;
    int32_t combo_count;
};

// Super Rotation System (https://harddrop.com/wiki/SRS)
struct SRSKickData {
    struct Kick { int x, y; };
    const Kick* kicks;
    const int length;
};

enum class Rotation : uint8_t {
    CW,
    CCW,
    HALF, // 180
    SIZE
};

extern const SRSKickData srs_table[int8_t(BlockType::SIZE)][4][uint8_t(Rotation::SIZE)];

namespace ops {

template<std::size_t N>
inline constexpr void placeRows(Board& board, const Rows<N>& rows, int x, int y) {
    const int x_offset = x << 1;
    for (std::size_t i = 0; i < N; ++i) { board.data[y + i] |= rows.data[i] >> x_offset; }
}
template<std::size_t N>
inline constexpr void removeRows(Board& board, const Rows<N>& rows, int x, int y) {
    const int x_offset = x << 1;
    for (std::size_t i = 0; i < N; ++i) { board.data[y + i] &= ~(rows.data[i] >> x_offset); }
}
template<std::size_t N>
inline constexpr bool canPlaceRows(const Board& board, const Rows<N>& rows, int x, int y) {
    const int x_offset = x << 1;
    for (std::size_t i = 0; i < N; ++i) {
        if ((board.data[y + i] & (rows.data[i] >> x_offset)) != 0) { return false; }
    }
    return true;
}

inline constexpr const Block& getBlock(BlockType type, int8_t orientation) { return blocks[int8_t(type)][orientation]; }
inline constexpr void placeBlock(Board& board, const Block& block, int x, int y) { placeRows(board, block, x, y); }
inline constexpr void removeBlock(Board& board, const Block& block, int x, int y) { removeRows(board, block, x, y); }
inline constexpr bool canPlaceBlock(const Board& board, const Block& block, int x, int y) { return canPlaceRows(board, block, x, y); }

} // namespace ops

void setSeed(State* state, uint32_t seed);

void reset(State* state);
bool generateBlock(State* state, bool called_by_hold = false);

bool moveLeft(State* state);
bool moveRight(State* state);
bool moveLeftToWall(State* state);
bool moveRightToWall(State* state);
bool softDrop(State* state);
bool softDropToFloor(State* state);
bool hardDrop(State* state);
bool rotateCounterclockwise(State* state);
bool rotateClockwise(State* state);
bool rotate180(State* state);
bool hold(State* state);

void toString(State* state, char* buf, size_t size);

void eraseCurrent(State* state);
bool pasteCurrent(State* state);
