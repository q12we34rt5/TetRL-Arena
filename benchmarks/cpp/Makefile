CXX ?= c++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic

PROJECT_ROOT := $(abspath $(CURDIR)/../..)
ENGINE_DIR := $(PROJECT_ROOT)/src/tetrl/csrc/engine
BUILD_DIR := $(PROJECT_ROOT)/build/benchmarks/cpp

COMMON_SRCS := $(ENGINE_DIR)/tetris.cpp
FAST_SRC := $(ENGINE_DIR)/placement_search.cpp
SLOW_SRC := $(ENGINE_DIR)/placement_search_slow.cpp
BENCH_SRC := $(CURDIR)/engine/placement_search_bench.cpp

INCLUDES := -I$(ENGINE_DIR)

FAST_BIN := $(BUILD_DIR)/placement_search_bench
SLOW_BIN := $(BUILD_DIR)/placement_search_slow_bench

.PHONY: all fast slow run-fast run-slow clean

all: fast slow

fast: $(FAST_BIN)

slow: $(SLOW_BIN)

run-fast: $(FAST_BIN)
	$(FAST_BIN) $(ARGS)

run-slow: $(SLOW_BIN)
	$(SLOW_BIN) $(ARGS)

$(FAST_BIN): $(BENCH_SRC) $(COMMON_SRCS) $(FAST_SRC) $(ENGINE_DIR)/placement_search.hpp $(ENGINE_DIR)/tetris.hpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DPLACEMENT_SEARCH_IMPL_NAME=\"fast\" $(BENCH_SRC) $(COMMON_SRCS) $(FAST_SRC) -o $@

$(SLOW_BIN): $(BENCH_SRC) $(COMMON_SRCS) $(SLOW_SRC) $(ENGINE_DIR)/placement_search.hpp $(ENGINE_DIR)/tetris.hpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DPLACEMENT_SEARCH_IMPL_NAME=\"slow\" $(BENCH_SRC) $(COMMON_SRCS) $(SLOW_SRC) -o $@

clean:
	rm -rf $(BUILD_DIR)
