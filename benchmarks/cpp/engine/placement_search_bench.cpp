#include "tetris.hpp"
#include "placement_search.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace tetrl;

#ifndef PLACEMENT_SEARCH_IMPL_NAME
#define PLACEMENT_SEARCH_IMPL_NAME "unknown"
#endif

int main(int argc, char* argv[]) {
    // Configuration
    double test_duration = 5.0;  // seconds
    int num_trials = 3;

    if (argc >= 2) test_duration = std::atof(argv[1]);
    if (argc >= 3) num_trials = std::atoi(argv[2]);

    printf("=== Placement Search Performance Test ===\n");
    printf("Implementation: %s\n", PLACEMENT_SEARCH_IMPL_NAME);
    printf("Test duration per trial: %.1f seconds\n", test_duration);
    printf("Number of trials: %d\n\n", num_trials);

    std::mt19937 rng(42);

    double total_steps_per_sec = 0.0;
    long long total_steps_all = 0;
    long long total_episodes_all = 0;

    for (int trial = 0; trial < num_trials; trial++) {
        State state;
        setSeed(&state, rng(), rng());
        reset(&state);

        long long step_count = 0;
        long long episode_count = 0;

        auto start = std::chrono::high_resolution_clock::now();
        double elapsed = 0.0;

        while (elapsed < test_duration) {
            if (!state.is_alive) {
                // Game Over immediately on spawn
                setSeed(&state, rng(), rng());
                reset(&state);
                episode_count++;
            } else {
                auto placements = findPlacements(state);

                if (placements.empty()) {
                    // Game Over
                    setSeed(&state, rng(), rng());
                    reset(&state);
                    episode_count++;
                } else {
                    // Pick a random placement
                    std::uniform_int_distribution<size_t> dist(0, placements.size() - 1);
                    size_t choice = dist(rng);
                    state = placements[choice].final_state;
                }
            }

            step_count++;

            // Check time every 1000 steps to reduce overhead (finding placements is heavier than basic steps)
            if (step_count % 1000 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                elapsed = std::chrono::duration<double>(now - start).count();
            }
        }

        // Final timing
        auto end = std::chrono::high_resolution_clock::now();
        double actual_elapsed = std::chrono::duration<double>(end - start).count();
        double steps_per_sec = step_count / actual_elapsed;

        printf("Trial %d: %lld steps in %.3f sec => %.0f steps/sec (placements/sec) | %lld episodes (avg %.0f steps/episode)\n",
               trial + 1, step_count, actual_elapsed, steps_per_sec,
               episode_count, episode_count > 0 ? (double)step_count / episode_count : 0.0);

        total_steps_per_sec += steps_per_sec;
        total_steps_all += step_count;
        total_episodes_all += episode_count;
    }

    printf("\n=== Summary ===\n");
    printf("Average: %.0f placements/sec\n", total_steps_per_sec / num_trials);
    printf("Total:   %lld steps, %lld episodes across all trials\n", total_steps_all, total_episodes_all);

    return 0;
}
