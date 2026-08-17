# NanoGPU simulator build.
#
# Usage:
#   make            build all test binaries into build/
#   make test       build and run all test binaries, stop on first failure
#   make clean      remove build/

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Isim/include

SRC_DIR  := sim/src
TEST_DIR := sim/test
BUILD_DIR := build

# MinGW/Windows g++ always produces a .exe regardless of -o; detect the
# platform so `make test` invokes the binary under the name that actually
# got created. $(OS) is a Make built-in, set to "Windows_NT" on Windows.
ifeq ($(OS),Windows_NT)
    EXE := .exe
else
    EXE :=
endif

TESTS := decoder_test exec_test vector_add_test
TEST_BINS := $(addprefix $(BUILD_DIR)/,$(addsuffix $(EXE),$(TESTS)))

.PHONY: all test clean

all: $(TEST_BINS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# decoder_test only needs the decoder -- it has no dependency on state.h.
$(BUILD_DIR)/decoder_test$(EXE): $(SRC_DIR)/decoder.cpp $(TEST_DIR)/decoder_test.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

# exec_test and vector_add_test both need the full chain: state + decoder + exec.
$(BUILD_DIR)/exec_test$(EXE): $(SRC_DIR)/state.cpp $(SRC_DIR)/decoder.cpp $(SRC_DIR)/exec.cpp $(TEST_DIR)/exec_test.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/vector_add_test$(EXE): $(SRC_DIR)/state.cpp $(SRC_DIR)/decoder.cpp $(SRC_DIR)/exec.cpp $(TEST_DIR)/vector_add_test.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: all
	@for t in $(TESTS); do \
		echo "=== $$t ==="; \
		"./$(BUILD_DIR)/$$t$(EXE)" || exit 1; \
		echo ""; \
	done
	@echo "All test binaries passed."

clean:
	rm -rf $(BUILD_DIR)