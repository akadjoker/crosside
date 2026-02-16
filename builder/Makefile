CXX ?= g++
MODE ?= release
TARGET := bin/builder
TEST_TARGET := bin/builder_tests

SRC := $(shell find src -name '*.cpp')
OBJ := $(patsubst src/%.cpp,obj/%.o,$(SRC))
CORE_OBJ := $(filter-out obj/main.o,$(OBJ))
DEP := $(OBJ:.o=.d)

TEST_DIR := tests
TEST_SRC := $(shell find $(TEST_DIR) -name '*.cpp' 2>/dev/null)
TEST_OBJ := $(patsubst $(TEST_DIR)/%.cpp,obj/tests/%.o,$(TEST_SRC))
GTEST_DIR := third_party/googletest/googletest
GTEST_SRC := $(GTEST_DIR)/src/gtest-all.cc
GTEST_OBJ := obj/gtest/gtest-all.o
TEST_DEP := $(TEST_OBJ:.o=.d) $(GTEST_OBJ:.o=.d)

CXXFLAGS_COMMON := -std=c++20 -Wall -Wextra -Iinclude -Ithird_party
CXXFLAGS_TEST := $(CXXFLAGS_COMMON) -I$(GTEST_DIR) -I$(GTEST_DIR)/include -I$(TEST_DIR)
LDFLAGS :=
TEST_LDFLAGS := $(LDFLAGS)

ifeq ($(OS),Windows_NT)
  LDFLAGS += -lws2_32
  TEST_LDFLAGS += -lws2_32
else
  TEST_LDFLAGS += -lpthread
endif

ifeq ($(MODE),debug)
  CXXFLAGS := $(CXXFLAGS_COMMON) -O0 -g3 -DDEBUG
  CXXFLAGS_TEST := $(CXXFLAGS_TEST) -O0 -g3 -DDEBUG
else
  CXXFLAGS := $(CXXFLAGS_COMMON) -O2 -DNDEBUG
  CXXFLAGS_TEST := $(CXXFLAGS_TEST) -O2 -DNDEBUG
endif

.PHONY: all release debug clean run test test-build

all: release

release:
	$(MAKE) MODE=release $(TARGET)

debug:
	$(MAKE) MODE=debug $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(TEST_TARGET): $(CORE_OBJ) $(TEST_OBJ) $(GTEST_OBJ)
	@mkdir -p $(dir $@)
	$(CXX) $(CORE_OBJ) $(TEST_OBJ) $(GTEST_OBJ) -o $@ $(TEST_LDFLAGS)

$(GTEST_OBJ): $(GTEST_SRC)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_TEST) -MMD -MP -c $< -o $@

obj/tests/%.o: $(TEST_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS_TEST) -MMD -MP -c $< -o $@

test-build:
	@if [ -z "$(TEST_SRC)" ]; then \
		echo "No test sources found in $(TEST_DIR)"; \
		exit 1; \
	fi
	@if [ ! -f "$(GTEST_SRC)" ]; then \
		echo "googletest not found at $(GTEST_DIR)."; \
		echo "Run: git clone --depth 1 https://github.com/google/googletest.git third_party/googletest"; \
		exit 1; \
	fi
	$(MAKE) MODE=$(MODE) $(TEST_TARGET)

test: test-build
	./$(TEST_TARGET)

run: $(TARGET)
	./$(TARGET) $(ARGS)

clean:
	rm -rf obj $(TARGET) $(TEST_TARGET) bin/crosside_cli

-include $(DEP) $(TEST_DEP)
