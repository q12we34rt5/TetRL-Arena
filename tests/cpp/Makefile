CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

PROJECT_ROOT := $(abspath $(CURDIR)/../..)
ENGINE_DIR := $(PROJECT_ROOT)/src/tetrl/csrc/engine
BUILD_DIR := $(PROJECT_ROOT)/build/tests/cpp

COMMON_SRCS := $(ENGINE_DIR)/tetris.cpp
FAST_SRC := $(ENGINE_DIR)/placement_search.cpp
SLOW_SRC := $(ENGINE_DIR)/placement_search_slow.cpp
TEST_SRC := $(CURDIR)/engine/placement_search_test.cpp

INCLUDES := -I$(ENGINE_DIR)

FAST_BIN := $(BUILD_DIR)/placement_search_fast_test
SLOW_BIN := $(BUILD_DIR)/placement_search_slow_test

.PHONY: all test test-fast test-slow clean

all: $(FAST_BIN) $(SLOW_BIN)

test: test-fast test-slow

test-fast: $(FAST_BIN)
	$(FAST_BIN)

test-slow: $(SLOW_BIN)
	$(SLOW_BIN)

$(FAST_BIN): $(TEST_SRC) $(COMMON_SRCS) $(FAST_SRC) $(ENGINE_DIR)/placement_search.hpp $(ENGINE_DIR)/tetris.hpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DPLACEMENT_SEARCH_IMPL_NAME=\"fast\" $(TEST_SRC) $(COMMON_SRCS) $(FAST_SRC) -o $@

$(SLOW_BIN): $(TEST_SRC) $(COMMON_SRCS) $(SLOW_SRC) $(ENGINE_DIR)/placement_search.hpp $(ENGINE_DIR)/tetris.hpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -DPLACEMENT_SEARCH_IMPL_NAME=\"slow\" $(TEST_SRC) $(COMMON_SRCS) $(SLOW_SRC) -o $@

clean:
	rm -rf $(BUILD_DIR)
