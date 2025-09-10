#pragma once
#include <cstdint>
#include <cstddef>

constexpr int BOARD_HEIGHT  = 32;
constexpr int BOARD_PADDING = 4;
constexpr int BOARD_TOP     = BOARD_HEIGHT - BOARD_PADDING - 21;
constexpr int BOARD_BOTTOM  = BOARD_TOP + 20;
constexpr int BOARD_LEFT    = 3;

// enum class CELL : uint32_t {
//     EMPTY  = 0b00'000000000000000000000000000000u,
//     BLOCK  = 0b01'000000000000000000000000000000u,
//     SHADOW = 0b10'000000000000000000000000000000u,
//     WALL   = 0b11'000000000000000000000000000000u,
// };

struct Block {
    enum Type : int8_t {
        NONE = -1,
        Z = 0, L, O, S, I, J, T,
        SIZE
    };
    uint32_t data[4];
};

enum class SpinType : uint8_t {
    NONE,
    SPIN,
    SPIN_MINI
};

struct State {
    uint32_t board[BOARD_HEIGHT];
    bool is_alive;
    Block::Type next[14];
    Block::Type hold;
    bool has_held;
    Block::Type current;
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

extern const Block blocks[Block::Type::SIZE][4];
extern const SRSKickData srs_table[Block::Type::SIZE][4][uint8_t(Rotation::SIZE)];

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
