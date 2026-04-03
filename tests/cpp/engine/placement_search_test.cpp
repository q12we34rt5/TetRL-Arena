#include "placement_search.hpp"
#include "tetris.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

using namespace tetrl;

#ifndef PLACEMENT_SEARCH_IMPL_NAME
#define PLACEMENT_SEARCH_IMPL_NAME "unknown"
#endif

namespace {

[[noreturn]] void fail(const std::string& message) {
    std::fprintf(stderr, "[%s] %s\n", PLACEMENT_SEARCH_IMPL_NAME, message.c_str());
    std::exit(1);
}

void check(bool condition, const std::string& message) {
    if (!condition) {
        fail(message);
    }
}

template <typename T, typename U>
void checkEqual(const T& actual, const U& expected, const std::string& message) {
    if (!(actual == expected)) {
        fail(message);
    }
}

struct PlacementKey {
    int lock_x;
    int lock_y;
    int orientation;
    int spin_type;

    bool operator<(const PlacementKey& other) const {
        return std::tie(lock_x, lock_y, orientation, spin_type) < std::tie(other.lock_x, other.lock_y, other.orientation, other.spin_type);
    }
};

State makeState(BlockType current) {
    State state {};
    setSeed(&state, 1u, 2u);
    reset(&state);
    state.current = current;
    state.orientation = 0;
    state.x = BLOCK_SPAWN_X;
    state.y = BLOCK_SPAWN_Y;
    state.srs_index = -1;
    state.was_last_rotation = false;
    state.spin_type = SpinType::NONE;
    state.is_alive = true;
    return state;
}

void setVisibleCell(State& state, int x, int y, Cell cell) {
    ops::setCell(state.board, BOARD_LEFT + x, BOARD_TOP + y, cell);
}

State makeSurfaceState(BlockType current) {
    State state = makeState(current);

    for (int x = 0; x < 10; ++x) {
        if (x != 4) {
            setVisibleCell(state, x, 19, Cell::GARBAGE);
        }
    }
    for (int x : {0, 1, 2, 7, 8, 9}) {
        setVisibleCell(state, x, 18, Cell::GARBAGE);
    }
    for (int x : {0, 9}) {
        setVisibleCell(state, x, 17, Cell::GARBAGE);
    }

    check(canPlaceCurrentBlock(&state), "surface fixture must leave the spawn position valid");
    return state;
}

bool statesEqual(const State& lhs, const State& rhs) {
    return std::memcmp(lhs.board.data, rhs.board.data, sizeof(lhs.board.data)) == 0
        && lhs.is_alive == rhs.is_alive
        && std::memcmp(lhs.next, rhs.next, sizeof(lhs.next)) == 0
        && lhs.hold == rhs.hold
        && lhs.has_held == rhs.has_held
        && lhs.current == rhs.current
        && lhs.orientation == rhs.orientation
        && lhs.x == rhs.x
        && lhs.y == rhs.y
        && lhs.seed == rhs.seed
        && lhs.srs_index == rhs.srs_index
        && lhs.piece_count == rhs.piece_count
        && lhs.was_last_rotation == rhs.was_last_rotation
        && lhs.spin_type == rhs.spin_type
        && lhs.perfect_clear == rhs.perfect_clear
        && lhs.back_to_back_count == rhs.back_to_back_count
        && lhs.combo_count == rhs.combo_count
        && lhs.lines_cleared == rhs.lines_cleared
        && lhs.attack == rhs.attack
        && lhs.lines_sent == rhs.lines_sent
        && lhs.total_lines_cleared == rhs.total_lines_cleared
        && lhs.total_attack == rhs.total_attack
        && lhs.total_lines_sent == rhs.total_lines_sent
        && lhs.garbage_seed == rhs.garbage_seed
        && std::memcmp(lhs.garbage_queue, rhs.garbage_queue, sizeof(lhs.garbage_queue)) == 0
        && std::memcmp(lhs.garbage_delay, rhs.garbage_delay, sizeof(lhs.garbage_delay)) == 0
        && lhs.max_garbage_spawn == rhs.max_garbage_spawn
        && lhs.garbage_blocking == rhs.garbage_blocking;
}

void applyActionOrFail(State& state, PlacementAction action) {
    bool success = false;
    switch (action) {
    case PlacementAction::LEFT:
        success = moveLeft(&state);
        break;
    case PlacementAction::RIGHT:
        success = moveRight(&state);
        break;
    case PlacementAction::LEFT_WALL:
        success = moveLeftToWall(&state);
        break;
    case PlacementAction::RIGHT_WALL:
        success = moveRightToWall(&state);
        break;
    case PlacementAction::SOFT_DROP:
        success = softDrop(&state);
        break;
    case PlacementAction::SOFT_DROP_FLOOR:
        success = softDropToFloor(&state);
        break;
    case PlacementAction::ROTATE_CW:
        success = rotateClockwise(&state);
        break;
    case PlacementAction::ROTATE_CCW:
        success = rotateCounterclockwise(&state);
        break;
    case PlacementAction::ROTATE_180:
        success = rotate180(&state);
        break;
    case PlacementAction::NONE:
    case PlacementAction::HARD_DROP:
        fail("placement paths must not contain NONE or HARD_DROP");
    }

    check(success, "every recorded action in a placement path must replay successfully");
}

void verifyPlacementsReplay(const State& initial_state, const std::vector<PlacementSearchResult>& placements) {
    check(!placements.empty(), "expected at least one reachable placement");

    std::set<PlacementKey> unique_keys;
    bool saw_empty_path = false;

    for (const PlacementSearchResult& placement : placements) {
        const PlacementKey key {
            placement.lock_x,
            placement.lock_y,
            static_cast<int>(placement.orientation),
            static_cast<int>(placement.spin_type),
        };
        check(unique_keys.insert(key).second, "placement results must be unique by lock position/orientation/spin");

        State replayed = initial_state;
        for (PlacementAction action : placement.path) {
            applyActionOrFail(replayed, action);
        }

        if (placement.path.empty()) {
            saw_empty_path = true;
        }

        State lock_probe = replayed;
        softDropToFloor(&lock_probe);

        checkEqual(static_cast<int>(lock_probe.x), static_cast<int>(placement.lock_x), "lock x must match the replayed path");
        checkEqual(static_cast<int>(lock_probe.y), static_cast<int>(placement.lock_y), "lock y must match the replayed path");
        checkEqual(static_cast<int>(lock_probe.orientation), static_cast<int>(placement.orientation), "lock orientation must match the replayed path");

        hardDrop(&replayed);
        check(statesEqual(replayed, placement.final_state), "replayed hard drop must exactly match the stored final state");
        checkEqual(static_cast<int>(replayed.spin_type), static_cast<int>(placement.spin_type), "spin type must match the replayed final state");
    }

    check(saw_empty_path, "the direct hard-drop placement from the initial state must be present");
}

void testRejectsDeadOrEmptyStates() {
    State dead_state = makeState(BlockType::T);
    dead_state.is_alive = false;
    check(findPlacements(dead_state).empty(), "dead states must produce no placements");

    State empty_state = makeState(BlockType::NONE);
    empty_state.current = BlockType::NONE;
    check(findPlacements(empty_state).empty(), "states without an active piece must produce no placements");
}

void testEmptyBoardIPlacementsReplay() {
    const State initial_state = makeState(BlockType::I);
    const std::vector<PlacementSearchResult> placements = findPlacements(initial_state);

    verifyPlacementsReplay(initial_state, placements);

    bool saw_rotation = false;
    bool saw_non_spawn_column = false;
    for (const PlacementSearchResult& placement : placements) {
        if (placement.orientation != 0) {
            saw_rotation = true;
        }
        if (placement.lock_x != initial_state.x) {
            saw_non_spawn_column = true;
        }
    }

    check(saw_rotation, "empty-board I placements should include rotated results");
    check(saw_non_spawn_column, "empty-board I placements should include lateral movement");
}

void testSurfaceTPPlacementsReplay() {
    const State initial_state = makeSurfaceState(BlockType::T);
    const std::vector<PlacementSearchResult> placements = findPlacements(initial_state);

    verifyPlacementsReplay(initial_state, placements);

    bool saw_line_clear = false;
    bool saw_rotation_path = false;
    for (const PlacementSearchResult& placement : placements) {
        if (placement.final_state.lines_cleared > 0) {
            saw_line_clear = true;
        }
        if (std::find_if(placement.path.begin(), placement.path.end(), [](PlacementAction action) {
                return action == PlacementAction::ROTATE_CW
                    || action == PlacementAction::ROTATE_CCW
                    || action == PlacementAction::ROTATE_180;
            }) != placement.path.end()) {
            saw_rotation_path = true;
        }
    }

    check(saw_line_clear, "surface T placements should include at least one line clear");
    check(saw_rotation_path, "surface T placements should include at least one rotation path");
}

} // namespace

int main() {
    std::printf("Running placement_search tests for %s implementation\n", PLACEMENT_SEARCH_IMPL_NAME);

    testRejectsDeadOrEmptyStates();
    testEmptyBoardIPlacementsReplay();
    testSurfaceTPPlacementsReplay();

    std::printf("[%s] all placement_search tests passed\n", PLACEMENT_SEARCH_IMPL_NAME);
    return 0;
}
