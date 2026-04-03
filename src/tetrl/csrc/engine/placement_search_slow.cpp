#include "placement_search.hpp"
#include <queue>
#include <set>
#include <tuple>
#include <cstring>
#include <cassert>
#include <algorithm>
#include <memory>

namespace tetrl {

/**
 * @brief Internal node to track visited states and parent pointers to reconstruct paths.
 */
struct VisitedNode {
    bool visited;
    PlacementAction action;
    std::int8_t parent_x;
    std::int8_t parent_y;
    std::int8_t parent_o;
    bool parent_w;
    std::int8_t parent_s;
};

/**
 * @brief Internal 5D array to track visited states during BFS.
 * 
 * Tracks `x` [-10, 21], `y` [-10, 53], `orientation` [0, 3], `was_last_rotation` [0, 1],
 * and `srs_index` [-1, 5] mapped to safe positive indices.
 */
struct VisitedArray {
    VisitedNode data[32][64][4][2][7];
    
    VisitedArray() {
        std::memset(data, 0, sizeof(data));
    }

    bool get(int x, int y, int o, bool w, int s) const {
        int ax = x + 10;
        int ay = y + 10;
        assert(ax >= 0 && ax < 32 && ay >= 0 && ay < 64 && o >= 0 && o < 4 && s >= -1 && s < 6);
        return data[ax][ay][o][w][s + 1].visited;
    }

    void set(int x, int y, int o, bool w, int s, PlacementAction action, int px, int py, int po, bool pw, int ps) {
        int ax = x + 10;
        int ay = y + 10;
        assert(ax >= 0 && ax < 32 && ay >= 0 && ay < 64 && o >= 0 && o < 4 && s >= -1 && s < 6);
        auto& node = data[ax][ay][o][w][s + 1];
        node.visited = true;
        node.action = action;
        node.parent_x = px;
        node.parent_y = py;
        node.parent_o = po;
        node.parent_w = pw;
        node.parent_s = ps;
    }

    void get_parent(int x, int y, int o, bool w, int s, PlacementAction& out_act, int& px, int& py, int& po, bool& pw, int& ps) const {
        int ax = x + 10;
        int ay = y + 10;
        const auto& node = data[ax][ay][o][w][s + 1];
        out_act = node.action;
        px = node.parent_x;
        py = node.parent_y;
        po = node.parent_o;
        pw = node.parent_w;
        ps = node.parent_s;
    }
};

std::vector<PlacementAction> buildPath(const std::unique_ptr<VisitedArray>& visited, int x, int y, int o, bool w, int s) {
    std::vector<PlacementAction> path;
    int cx = x, cy = y, co = o, cs = s;
    bool cw = w;
    while (true) {
        PlacementAction act;
        int px, py, po, ps;
        bool pw;
        visited->get_parent(cx, cy, co, cw, cs, act, px, py, po, pw, ps);
        if (act == PlacementAction::NONE) {
            break;
        }
        path.push_back(act);
        cx = px; cy = py; co = po; cw = pw; cs = ps;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

std::vector<PlacementSearchResult> findPlacements(const State& initial_state) {
    std::vector<PlacementSearchResult> results;
    
    if (initial_state.current == BlockType::NONE || !initial_state.is_alive) {
        return results;
    }

    std::unique_ptr<VisitedArray> visited = std::make_unique<VisitedArray>();
    std::queue<State> q;

    // Helper to enqueue theoretically unique un-locked structural states
    auto tryEnqueue = [&](const State& st, PlacementAction act, const State& parent) {
        if (!visited->get(st.x, st.y, st.orientation, st.was_last_rotation, st.srs_index)) {
            visited->set(st.x, st.y, st.orientation, st.was_last_rotation, st.srs_index,
                         act, parent.x, parent.y, parent.orientation, parent.was_last_rotation, parent.srs_index);
            q.push(st);
        }
    };

    visited->set(initial_state.x, initial_state.y, initial_state.orientation, initial_state.was_last_rotation, initial_state.srs_index,
                 PlacementAction::NONE, 0, 0, 0, false, 0);
    q.push(initial_state);

    // Track unique locked placements defined by (lock_x, lock_y, orientation, spin_type)
    std::set<std::tuple<int, int, int, SpinType>> seen_placements;

    while (!q.empty()) {
        State curr = q.front();
        q.pop();

        // 1. Determine the exact locked location using softDropToFloor()
        State lock_probe = curr;
        softDropToFloor(&lock_probe);
        int lock_x = lock_probe.x;
        int lock_y = lock_probe.y;
        int lock_o = lock_probe.orientation;

        // 2. Perform the actual Drop and Lock evaluation to grab spin_type and full aftermath
        State final_st = curr;
        hardDrop(&final_st); // Executes lock, evaluates spin_type, spawns next piece
        SpinType stype = final_st.spin_type;

        auto placement_key = std::make_tuple(lock_x, lock_y, lock_o, stype);
        if (seen_placements.find(placement_key) == seen_placements.end()) {
            seen_placements.insert(placement_key);
            
            std::vector<PlacementAction> path = buildPath(visited, curr.x, curr.y, curr.orientation, curr.was_last_rotation, curr.srs_index);
            
            PlacementSearchResult res;
            res.lock_x = static_cast<std::int8_t>(lock_x);
            res.lock_y = static_cast<std::int8_t>(lock_y);
            res.orientation = static_cast<std::uint8_t>(lock_o);
            res.spin_type = stype;
            res.final_state = final_st;
            res.path = std::move(path);
            results.push_back(res);
        }

        // 3. Expand the structural boundaries 1-step at a time
        // (Movements only happen on floating active pieces, not the locked variants.)
        { State next = curr; if (moveLeft(&next)) tryEnqueue(next, PlacementAction::LEFT, curr); }
        { State next = curr; if (moveRight(&next)) tryEnqueue(next, PlacementAction::RIGHT, curr); }
        { State next = curr; if (moveLeftToWall(&next)) tryEnqueue(next, PlacementAction::LEFT_WALL, curr); }
        { State next = curr; if (moveRightToWall(&next)) tryEnqueue(next, PlacementAction::RIGHT_WALL, curr); }
        { State next = curr; if (softDrop(&next)) tryEnqueue(next, PlacementAction::SOFT_DROP, curr); }
        { State next = curr; if (softDropToFloor(&next)) tryEnqueue(next, PlacementAction::SOFT_DROP_FLOOR, curr); }
        { State next = curr; if (rotateClockwise(&next)) tryEnqueue(next, PlacementAction::ROTATE_CW, curr); }
        { State next = curr; if (rotateCounterclockwise(&next)) tryEnqueue(next, PlacementAction::ROTATE_CCW, curr); }
        { State next = curr; if (rotate180(&next)) tryEnqueue(next, PlacementAction::ROTATE_180, curr); }
    }

    return results;
}

} // namespace tetrl
