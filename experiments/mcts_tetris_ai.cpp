#include "argparser.hpp"
#include "placement_search.hpp"
#include "tetris.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace tetrl {
namespace {

constexpr int kVisibleWidth = BOARD_RIGHT - BOARD_LEFT + 1;
constexpr std::size_t kBoardStringCapacity = 2048;

struct MctsConfig {
    std::uint32_t seed = 1u;
    std::uint32_t garbage_seed = 2u;
    int iterations = 800;
    int rollout_depth = 6;
    int max_pieces = 200;
    int board_every = 0;
    int num_threads = 1;
    double exploration = 1.35;
    double virtual_loss = 0.5;
    double rollout_discount = 0.90;
    double rollout_epsilon = 0.10;
    double rollout_temperature = 8.0;
    double utility_scale = 600.0;
    bool allow_hold = true;
    bool verbose = false;
};

struct BoardFeatures {
    std::array<int, kVisibleWidth> heights {};
    int aggregate_height = 0;
    int max_height = 0;
    int holes = 0;
    int bumpiness = 0;
    int wells = 0;
    int row_transitions = 0;
    int col_transitions = 0;
    int hidden_blocks = 0;
    int danger_cells = 0;
};

struct SearchAction {
    bool used_hold = false;
    BlockType placed_piece = BlockType::NONE;
    PlacementSearchResult placement;
    double heuristic_value = -std::numeric_limits<double>::infinity();
};

struct Node {
    State state {};
    int parent = -1;
    int incoming_action_index = -1;
    bool initialized = false;
    bool terminal = false;
    int visits = 0;
    double total_value = 0.0;
    std::size_t next_unexpanded = 0;
    std::vector<SearchAction> actions;
    std::vector<int> children_by_action;
};

struct RootActionStats {
    SearchAction action;
    int visits = 0;
    double mean_utility = 0.0;
};

struct Decision {
    bool valid = false;
    SearchAction action;
    int visits = 0;
    double mean_utility = 0.0;
    std::vector<RootActionStats> root_stats;
};

struct RuntimeStats {
    double elapsed_seconds = 0.0;
    double pps = 0.0;
    double apm = 0.0;
    double app = 0.0;
};

bool isOccupied(const State& state, int board_x, int board_y) {
    return ops::getCell(state.board, board_x, board_y) != Cell::EMPTY;
}

BoardFeatures extractBoardFeatures(const State& state) {
    BoardFeatures features;

    for (int x = 0; x < kVisibleWidth; ++x) {
        const int board_x = BOARD_LEFT + x;
        int first_filled_y = -1;
        bool seen_filled = false;

        for (int board_y = BOARD_TOP; board_y <= BOARD_BOTTOM; ++board_y) {
            const bool occupied = isOccupied(state, board_x, board_y);
            if (occupied && !seen_filled) {
                seen_filled = true;
                first_filled_y = board_y;
            } else if (!occupied && seen_filled) {
                features.holes++;
            }
        }

        if (seen_filled) {
            const int height = BOARD_BOTTOM - first_filled_y + 1;
            features.heights[static_cast<std::size_t>(x)] = height;
            features.aggregate_height += height;
            features.max_height = std::max(features.max_height, height);
        }
    }

    for (int x = 0; x + 1 < kVisibleWidth; ++x) {
        features.bumpiness += std::abs(features.heights[static_cast<std::size_t>(x)] - features.heights[static_cast<std::size_t>(x + 1)]);
    }

    for (int board_y = BOARD_TOP; board_y <= BOARD_BOTTOM; ++board_y) {
        bool prev_occupied = true;
        for (int x = 0; x < kVisibleWidth; ++x) {
            const bool occupied = isOccupied(state, BOARD_LEFT + x, board_y);
            features.row_transitions += static_cast<int>(occupied != prev_occupied);
            prev_occupied = occupied;
        }
        features.row_transitions += static_cast<int>(prev_occupied != true);
    }

    for (int x = 0; x < kVisibleWidth; ++x) {
        bool prev_occupied = false;
        int well_depth = 0;

        for (int board_y = BOARD_TOP; board_y <= BOARD_BOTTOM; ++board_y) {
            const bool occupied = isOccupied(state, BOARD_LEFT + x, board_y);
            features.col_transitions += static_cast<int>(occupied != prev_occupied);
            prev_occupied = occupied;

            const bool left_occupied = (x == 0) || isOccupied(state, BOARD_LEFT + x - 1, board_y);
            const bool right_occupied = (x == kVisibleWidth - 1) || isOccupied(state, BOARD_LEFT + x + 1, board_y);
            if (!occupied && left_occupied && right_occupied) {
                ++well_depth;
                features.wells += well_depth;
            } else {
                well_depth = 0;
            }
        }

        features.col_transitions += static_cast<int>(prev_occupied != true);
    }

    for (int board_y = 0; board_y < BOARD_TOP; ++board_y) {
        for (int board_x = BOARD_LEFT; board_x <= BOARD_RIGHT; ++board_x) {
            if (isOccupied(state, board_x, board_y)) {
                features.hidden_blocks++;
            }
        }
    }

    for (int board_y = BOARD_TOP; board_y < BOARD_TOP + 4; ++board_y) {
        for (int board_x = BOARD_LEFT; board_x <= BOARD_RIGHT; ++board_x) {
            if (isOccupied(state, board_x, board_y)) {
                features.danger_cells++;
            }
        }
    }

    return features;
}

double evaluateState(const State& state) {
    if (!state.is_alive) {
        return -2500.0;
    }

    const BoardFeatures features = extractBoardFeatures(state);

    double score = 0.0;
    score += 28.0 * static_cast<double>(state.attack);
    score += 6.0 * static_cast<double>(state.lines_cleared);
    score += 4.0 * static_cast<double>(state.lines_sent);
    score += 5.0 * static_cast<double>(std::max(0, state.combo_count));
    score += 3.0 * static_cast<double>(std::max(0, state.back_to_back_count));
    score += state.perfect_clear ? 80.0 : 0.0;

    if (state.lines_cleared > 0) {
        if (state.spin_type == SpinType::SPIN) {
            score += 18.0;
        } else if (state.spin_type == SpinType::SPIN_MINI) {
            score += 8.0;
        }
    }

    score -= 4.5 * static_cast<double>(features.aggregate_height);
    score -= 17.0 * static_cast<double>(features.holes);
    score -= 2.5 * static_cast<double>(features.bumpiness);
    score -= 9.0 * static_cast<double>(features.max_height);
    score -= 1.4 * static_cast<double>(features.wells);
    score -= 1.0 * static_cast<double>(features.row_transitions);
    score -= 0.8 * static_cast<double>(features.col_transitions);
    score -= 14.0 * static_cast<double>(features.hidden_blocks);
    score -= 8.0 * static_cast<double>(features.danger_cells);

    return score;
}

const char* blockTypeName(BlockType type) {
    switch (type) {
    case BlockType::Z: return "Z";
    case BlockType::L: return "L";
    case BlockType::O: return "O";
    case BlockType::S: return "S";
    case BlockType::I: return "I";
    case BlockType::J: return "J";
    case BlockType::T: return "T";
    case BlockType::NONE:
    case BlockType::SIZE:
        break;
    }
    return "NONE";
}

const char* spinTypeName(SpinType spin_type) {
    switch (spin_type) {
    case SpinType::NONE: return "none";
    case SpinType::SPIN: return "spin";
    case SpinType::SPIN_MINI: return "mini";
    }
    return "none";
}

const char* placementActionName(PlacementAction action) {
    switch (action) {
    case PlacementAction::NONE: return "NONE";
    case PlacementAction::LEFT: return "LEFT";
    case PlacementAction::RIGHT: return "RIGHT";
    case PlacementAction::LEFT_WALL: return "LEFT_WALL";
    case PlacementAction::RIGHT_WALL: return "RIGHT_WALL";
    case PlacementAction::SOFT_DROP: return "SOFT_DROP";
    case PlacementAction::SOFT_DROP_FLOOR: return "SOFT_DROP_FLOOR";
    case PlacementAction::HARD_DROP: return "HARD_DROP";
    case PlacementAction::ROTATE_CW: return "ROTATE_CW";
    case PlacementAction::ROTATE_CCW: return "ROTATE_CCW";
    case PlacementAction::ROTATE_180: return "ROTATE_180";
    }
    return "UNKNOWN";
}

std::string formatPath(const SearchAction& action) {
    std::ostringstream oss;
    bool first = true;

    if (action.used_hold) {
        oss << "HOLD";
        first = false;
    }

    for (PlacementAction step : action.placement.path) {
        if (!first) {
            oss << " -> ";
        }
        oss << placementActionName(step);
        first = false;
    }

    if (!first) {
        oss << " -> ";
    }
    oss << "HARD_DROP";
    return oss.str();
}

std::string stateToString(const State& state) {
    std::array<char, kBoardStringCapacity> buffer {};
    State copy = state;
    toString(&copy, buffer.data(), buffer.size());
    return std::string(buffer.data(), strnlen(buffer.data(), buffer.size()));
}

BlockType placedBlockTypeForAction(const State& state, bool used_hold) {
    if (!used_hold) {
        return state.current;
    }
    return state.hold != BlockType::NONE ? state.hold : state.next[0];
}

std::vector<SearchAction> enumerateActions(const State& state, bool allow_hold) {
    std::vector<SearchAction> actions;

    const auto append_actions = [&](const State& source_state, bool used_hold, BlockType placed_piece) {
        std::vector<PlacementSearchResult> placements = findPlacements(source_state);
        actions.reserve(actions.size() + placements.size());
        for (PlacementSearchResult& placement : placements) {
            SearchAction action;
            action.used_hold = used_hold;
            action.placed_piece = placed_piece;
            action.heuristic_value = evaluateState(placement.final_state);
            action.placement = std::move(placement);
            actions.push_back(std::move(action));
        }
    };

    if (state.is_alive && state.current != BlockType::NONE) {
        append_actions(state, false, state.current);
    }

    if (allow_hold && state.is_alive && state.current != BlockType::NONE && !state.has_held) {
        State hold_state = state;
        hold(&hold_state);
        if (hold_state.is_alive && hold_state.current != BlockType::NONE) {
            append_actions(hold_state, true, placedBlockTypeForAction(state, true));
        }
    }

    std::stable_sort(actions.begin(), actions.end(), [](const SearchAction& lhs, const SearchAction& rhs) {
        if (lhs.heuristic_value != rhs.heuristic_value) {
            return lhs.heuristic_value > rhs.heuristic_value;
        }
        if (lhs.used_hold != rhs.used_hold) {
            return lhs.used_hold < rhs.used_hold;
        }
        return lhs.placement.path.size() < rhs.placement.path.size();
    });

    return actions;
}

void initializeNode(Node& node, bool allow_hold) {
    if (node.initialized) {
        return;
    }

    node.actions = enumerateActions(node.state, allow_hold);
    node.children_by_action.assign(node.actions.size(), -1);
    node.terminal = node.actions.empty();
    node.next_unexpanded = 0;
    node.initialized = true;
}

int addChild(std::vector<Node>& nodes, int parent_index, int action_index, bool allow_hold) {
    Node child;
    child.state = nodes[static_cast<std::size_t>(parent_index)].actions[static_cast<std::size_t>(action_index)].placement.final_state;
    child.parent = parent_index;
    child.incoming_action_index = action_index;
    nodes.push_back(std::move(child));
    const int child_index = static_cast<int>(nodes.size() - 1);
    nodes[static_cast<std::size_t>(parent_index)].children_by_action[static_cast<std::size_t>(action_index)] = child_index;
    initializeNode(nodes.back(), allow_hold);
    return child_index;
}

int selectChildUct(const Node& node, const std::vector<Node>& nodes, double exploration) {
    if (node.visits <= 0) {
        return -1;
    }

    const double log_parent_visits = std::log(static_cast<double>(node.visits));
    int best_child_index = -1;
    double best_score = -std::numeric_limits<double>::infinity();

    for (std::size_t action_index = 0; action_index < node.children_by_action.size(); ++action_index) {
        const int child_index = node.children_by_action[action_index];
        if (child_index < 0) {
            continue;
        }

        const Node& child = nodes[static_cast<std::size_t>(child_index)];
        if (child.visits <= 0) {
            return child_index;
        }

        const double exploitation = child.total_value / static_cast<double>(child.visits);
        const double exploration_bonus = exploration * std::sqrt(log_parent_visits / static_cast<double>(child.visits));
        const double heuristic_bonus = 0.15 * std::tanh(node.actions[action_index].heuristic_value / 60.0) / (1.0 + child.visits);
        const double score = exploitation + exploration_bonus + heuristic_bonus;

        if (score > best_score) {
            best_score = score;
            best_child_index = child_index;
        }
    }

    return best_child_index;
}

std::size_t chooseRolloutAction(const std::vector<SearchAction>& actions, const MctsConfig& config, std::mt19937& rng) {
    if (actions.size() == 1) {
        return 0;
    }

    std::uniform_real_distribution<double> unit_dist(0.0, 1.0);
    if (unit_dist(rng) < config.rollout_epsilon) {
        std::uniform_int_distribution<std::size_t> index_dist(0, actions.size() - 1);
        return index_dist(rng);
    }

    const std::size_t candidate_count = std::min<std::size_t>(4, actions.size());
    const double temperature = std::max(0.001, config.rollout_temperature);

    double max_value = actions.front().heuristic_value;
    std::vector<double> weights(candidate_count, 0.0);
    double weight_sum = 0.0;

    for (std::size_t i = 0; i < candidate_count; ++i) {
        weights[i] = std::exp((actions[i].heuristic_value - max_value) / temperature);
        weight_sum += weights[i];
    }

    std::uniform_real_distribution<double> pick_dist(0.0, weight_sum);
    double target = pick_dist(rng);
    for (std::size_t i = 0; i < candidate_count; ++i) {
        target -= weights[i];
        if (target <= 0.0) {
            return i;
        }
    }

    return candidate_count - 1;
}

double rolloutValue(State state, const MctsConfig& config, std::mt19937& rng) {
    if (!state.is_alive) {
        return -2500.0;
    }

    double total = evaluateState(state);
    double discount = config.rollout_discount;

    for (int depth = 0; depth < config.rollout_depth; ++depth) {
        std::vector<SearchAction> actions = enumerateActions(state, config.allow_hold);
        if (actions.empty()) {
            break;
        }

        const std::size_t chosen = chooseRolloutAction(actions, config, rng);
        state = actions[chosen].placement.final_state;
        total += discount * evaluateState(state);
        discount *= config.rollout_discount;

        if (!state.is_alive) {
            break;
        }
    }

    return total;
}

Decision chooseActionMcts(const State& root_state, const MctsConfig& config, std::uint32_t rng_seed) {
    Decision decision;

    Node root;
    root.state = root_state;
    initializeNode(root, config.allow_hold);
    if (root.terminal) {
        return decision;
    }

    std::vector<Node> nodes;
    nodes.reserve(static_cast<std::size_t>(config.iterations) + 1);
    nodes.push_back(std::move(root));

    std::mutex tree_mutex;

    auto worker = [&](int worker_id, int iteration_begin, int iteration_end) {
        const std::uint32_t worker_seed = rng_seed
            ^ (0x9e3779b9u * static_cast<std::uint32_t>(worker_id + 1))
            ^ (0x85ebca6bu + static_cast<std::uint32_t>(iteration_begin * 17));
        std::mt19937 worker_rng(worker_seed);

        for (int iteration = iteration_begin; iteration < iteration_end; ++iteration) {
            std::vector<int> path;
            State leaf_state {};

            {
                std::lock_guard<std::mutex> lock(tree_mutex);

                int node_index = 0;
                path.push_back(0);

                while (true) {
                    Node& node = nodes[static_cast<std::size_t>(node_index)];

                    if (node.terminal) {
                        break;
                    }

                    if (node.next_unexpanded < node.actions.size()) {
                        const int action_index = static_cast<int>(node.next_unexpanded++);
                        node_index = addChild(nodes, node_index, action_index, config.allow_hold);
                        path.push_back(node_index);
                        break;
                    }

                    const int next_index = selectChildUct(node, nodes, config.exploration);
                    if (next_index < 0) {
                        break;
                    }

                    node_index = next_index;
                    path.push_back(node_index);
                }

                for (int visited_index : path) {
                    Node& visited_node = nodes[static_cast<std::size_t>(visited_index)];
                    visited_node.visits++;
                    visited_node.total_value -= config.virtual_loss;
                }

                leaf_state = nodes[static_cast<std::size_t>(path.back())].state;
            }

            const double raw_value = rolloutValue(leaf_state, config, worker_rng);
            const double utility = raw_value / (config.utility_scale + std::abs(raw_value));

            {
                std::lock_guard<std::mutex> lock(tree_mutex);
                for (int visited_index : path) {
                    Node& visited_node = nodes[static_cast<std::size_t>(visited_index)];
                    visited_node.total_value += config.virtual_loss + utility;
                }
            }
        }
    };

    const int worker_count = std::max(1, std::min(config.num_threads, std::max(1, config.iterations)));
    const int base_iterations = config.iterations / worker_count;
    const int extra_iterations = config.iterations % worker_count;

    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(std::max(0, worker_count - 1)));

    int iteration_begin = 0;
    for (int worker_id = 0; worker_id < worker_count; ++worker_id) {
        const int iteration_count = base_iterations + (worker_id < extra_iterations ? 1 : 0);
        const int iteration_end = iteration_begin + iteration_count;
        if (worker_id + 1 == worker_count) {
            worker(worker_id, iteration_begin, iteration_end);
        } else {
            threads.emplace_back(worker, worker_id, iteration_begin, iteration_end);
        }
        iteration_begin = iteration_end;
    }

    for (std::thread& thread : threads) {
        thread.join();
    }

    const Node& root_node = nodes.front();
    decision.root_stats.reserve(root_node.actions.size());

    for (std::size_t action_index = 0; action_index < root_node.actions.size(); ++action_index) {
        RootActionStats stats;
        stats.action = root_node.actions[action_index];
        const int child_index = root_node.children_by_action[action_index];
        if (child_index >= 0) {
            const Node& child = nodes[static_cast<std::size_t>(child_index)];
            stats.visits = child.visits;
            stats.mean_utility = child.visits > 0 ? child.total_value / static_cast<double>(child.visits) : 0.0;
        }
        decision.root_stats.push_back(std::move(stats));
    }

    std::stable_sort(decision.root_stats.begin(), decision.root_stats.end(), [](const RootActionStats& lhs, const RootActionStats& rhs) {
        if (lhs.visits != rhs.visits) {
            return lhs.visits > rhs.visits;
        }
        if (lhs.mean_utility != rhs.mean_utility) {
            return lhs.mean_utility > rhs.mean_utility;
        }
        return lhs.action.heuristic_value > rhs.action.heuristic_value;
    });

    const RootActionStats& best = decision.root_stats.front();
    decision.valid = true;
    decision.action = best.action;
    decision.visits = best.visits;
    decision.mean_utility = best.mean_utility;
    return decision;
}

RuntimeStats computeRuntimeStats(const State& state, double elapsed_seconds) {
    RuntimeStats stats;
    stats.elapsed_seconds = elapsed_seconds;

    if (elapsed_seconds > 0.0) {
        stats.pps = static_cast<double>(state.piece_count) / elapsed_seconds;
        stats.apm = static_cast<double>(state.total_attack) * 60.0 / elapsed_seconds;
    }
    if (state.piece_count > 0) {
        stats.app = static_cast<double>(state.total_attack) / static_cast<double>(state.piece_count);
    }
    return stats;
}

void printUsage(const char* argv0) {
    std::cerr
        << "Usage: " << argv0 << " [options]\n"
        << "Options:\n"
        << "  --seed N              Engine random seed (default 1)\n"
        << "  --garbage-seed N      Garbage random seed (default 2)\n"
        << "  --iterations N        MCTS iterations per move (default 800)\n"
        << "  --rollout-depth N     Rollout depth in placements (default 6)\n"
        << "  --max-pieces N        Stop after N placed pieces (default 200)\n"
        << "  --threads N           Number of MCTS workers (default 1)\n"
        << "  --exploration X       UCT exploration constant (default 1.35)\n"
        << "  --virtual-loss X      Virtual loss per in-flight path (default 0.5)\n"
        << "  --utility-scale X     Value normalization factor (default 600)\n"
        << "  --board-every N       Print board every N moves, 0 disables\n"
        << "  --no-hold             Disable hold branch expansion\n"
        << "  --verbose             Print root candidate summaries each move\n"
        << "  --help                Show this message\n";
}

bool parseArgs(int argc, char** argv, MctsConfig& config) {
    ArgCLITool::ArgParser parser;
    parser.add("--seed").nvalues(1).defaultValues({std::to_string(config.seed)});
    parser.add("--garbage-seed").nvalues(1).defaultValues({std::to_string(config.garbage_seed)});
    parser.add("--iterations").nvalues(1).defaultValues({std::to_string(config.iterations)});
    parser.add("--rollout-depth").nvalues(1).defaultValues({std::to_string(config.rollout_depth)});
    parser.add("--max-pieces").nvalues(1).defaultValues({std::to_string(config.max_pieces)});
    parser.add("--threads").nvalues(1).defaultValues({std::to_string(config.num_threads)});
    parser.add("--exploration").nvalues(1).defaultValues({std::to_string(config.exploration)});
    parser.add("--virtual-loss").nvalues(1).defaultValues({std::to_string(config.virtual_loss)});
    parser.add("--utility-scale").nvalues(1).defaultValues({std::to_string(config.utility_scale)});
    parser.add("--board-every").nvalues(1).defaultValues({std::to_string(config.board_every)});
    parser.add("--no-hold");
    parser.add("--verbose");
    parser.add("-h", "--help");

    try {
        ArgCLITool::Args args = parser.parse(argc, argv);
        if (args["--help"]) {
            printUsage(argv[0]);
            std::exit(0);
        }

        config.seed = args["--seed"].as<std::uint32_t>();
        config.garbage_seed = args["--garbage-seed"].as<std::uint32_t>();
        config.iterations = args["--iterations"].as<int>();
        config.rollout_depth = args["--rollout-depth"].as<int>();
        config.max_pieces = args["--max-pieces"].as<int>();
        config.num_threads = args["--threads"].as<int>();
        config.exploration = args["--exploration"].as<double>();
        config.virtual_loss = args["--virtual-loss"].as<double>();
        config.utility_scale = args["--utility-scale"].as<double>();
        config.board_every = args["--board-every"].as<int>();
        if (args["--no-hold"]) {
            config.allow_hold = false;
        }
        if (args["--verbose"]) {
            config.verbose = true;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << "\n";
        return false;
    }

    config.iterations = std::max(0, config.iterations);
    config.rollout_depth = std::max(0, config.rollout_depth);
    config.max_pieces = std::max(1, config.max_pieces);
    config.board_every = std::max(0, config.board_every);
    config.num_threads = std::max(1, config.num_threads);
    config.exploration = std::max(0.0, config.exploration);
    config.virtual_loss = std::max(0.0, config.virtual_loss);
    config.utility_scale = std::max(1.0, config.utility_scale);
    return true;
}

void printDecisionSummary(
    int move_index,
    const Decision& decision,
    const RuntimeStats& runtime_stats,
    bool verbose
) {
    const SearchAction& action = decision.action;
    const State& next_state = action.placement.final_state;

    std::cout
        << "move=" << move_index
        << " place=" << blockTypeName(action.placed_piece)
        << " hold=" << (action.used_hold ? "yes" : "no")
        << " visits=" << decision.visits
        << " value=" << std::fixed << std::setprecision(3) << decision.mean_utility
        << " lines=" << next_state.lines_cleared
        << " attack=" << next_state.attack
        << " spin=" << spinTypeName(next_state.spin_type)
        << " pps=" << std::fixed << std::setprecision(2) << runtime_stats.pps
        << " apm=" << std::fixed << std::setprecision(2) << runtime_stats.apm
        << " app=" << std::fixed << std::setprecision(2) << runtime_stats.app
        << " path=" << formatPath(action)
        << "\n";

    if (!verbose) {
        return;
    }

    const std::size_t count = std::min<std::size_t>(5, decision.root_stats.size());
    for (std::size_t i = 0; i < count; ++i) {
        const RootActionStats& stats = decision.root_stats[i];
        const State& child_state = stats.action.placement.final_state;
        std::cout
            << "  candidate[" << i << "]"
            << " place=" << blockTypeName(stats.action.placed_piece)
            << " hold=" << (stats.action.used_hold ? "yes" : "no")
            << " visits=" << stats.visits
            << " mean=" << std::fixed << std::setprecision(3) << stats.mean_utility
            << " heuristic=" << std::fixed << std::setprecision(1) << stats.action.heuristic_value
            << " lines=" << child_state.lines_cleared
            << " attack=" << child_state.attack
            << " spin=" << spinTypeName(child_state.spin_type)
            << " path=" << formatPath(stats.action)
            << "\n";
    }
}

} // namespace
} // namespace tetrl

int main(int argc, char** argv) {
    using namespace tetrl;

    MctsConfig config;
    if (!parseArgs(argc, argv, config)) {
        printUsage(argv[0]);
        return 1;
    }

    State state {};
    setSeed(&state, config.seed, config.garbage_seed);
    reset(&state);

    const std::uint32_t mcts_seed = config.seed ^ (config.garbage_seed * 0x9e3779b9u) ^ 0x85ebca6bu;
    std::mt19937 rng(mcts_seed);
    const auto start_time = std::chrono::steady_clock::now();

    std::cout
        << "seed=" << config.seed
        << " garbage_seed=" << config.garbage_seed
        << " iterations=" << config.iterations
        << " threads=" << config.num_threads
        << " virtual_loss=" << config.virtual_loss
        << " rollout_depth=" << config.rollout_depth
        << " hold=" << (config.allow_hold ? "on" : "off")
        << "\n";

    for (int move_index = 1; state.is_alive && move_index <= config.max_pieces; ++move_index) {
        const Decision decision = chooseActionMcts(state, config, rng());
        if (!decision.valid) {
            std::cout << "no legal actions remain\n";
            break;
        }

        state = decision.action.placement.final_state;
        const auto now = std::chrono::steady_clock::now();
        const double elapsed_seconds = std::chrono::duration<double>(now - start_time).count();
        const RuntimeStats runtime_stats = computeRuntimeStats(state, elapsed_seconds);
        printDecisionSummary(move_index, decision, runtime_stats, config.verbose);

        if (config.board_every > 0 && (move_index % config.board_every) == 0) {
            std::cout << stateToString(state) << "\n";
        }
    }

    std::cout
        << "game_over=" << (!state.is_alive ? "yes" : "no")
        << " pieces=" << state.piece_count
        << " total_lines=" << state.total_lines_cleared
        << " total_attack=" << state.total_attack
        << " total_sent=" << state.total_lines_sent
        << "\n";

    if (config.board_every == 0) {
        std::cout << stateToString(state) << "\n";
    }

    return 0;
}
