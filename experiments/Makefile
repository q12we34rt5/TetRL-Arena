CXX ?= c++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic

PROJECT_ROOT := $(abspath $(CURDIR)/..)
ENGINE_DIR := $(PROJECT_ROOT)/src/tetrl/csrc/engine
THIRD_PARTY_DIR := $(CURDIR)/third_party
BUILD_DIR := $(PROJECT_ROOT)/build/experiments

AI_SRC := $(CURDIR)/mcts_tetris_ai.cpp
ENGINE_SRCS := $(ENGINE_DIR)/tetris.cpp $(ENGINE_DIR)/placement_search.cpp
INCLUDES := -I$(ENGINE_DIR) -I$(THIRD_PARTY_DIR)

AI_BIN := $(BUILD_DIR)/mcts_tetris_ai

.PHONY: all run clean

all: $(AI_BIN)

run: $(AI_BIN)
	$(AI_BIN) $(ARGS)

$(AI_BIN): $(AI_SRC) $(ENGINE_SRCS) $(ENGINE_DIR)/tetris.hpp $(ENGINE_DIR)/placement_search.hpp $(THIRD_PARTY_DIR)/argparser.hpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(AI_SRC) $(ENGINE_SRCS) -o $@

clean:
	rm -rf $(BUILD_DIR)
