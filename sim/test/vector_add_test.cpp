#include <iostream>
#include "state.h"
#include "decoder.h"
#include "exec.h"

void print_test_result(const std::string& name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name << "\n";
    if (!passed) std::exit(1);
}

void run_to_halt(gpuState& state, int max_cycles = 100) {
    int cycles = 0;
    while (!state.halted && cycles < max_cycles) {
        step(state);
        ++cycles;
    }
    print_test_result("Program halted within cycle budget", state.halted);
}

// ---------------------------------------------------------------------
// First real kernel: 8-wide vector add.
//
//   result[tid] = a[tid] + b[tid]     for tid = 0..7, all in parallel
//
// Memory layout (chosen for this test):
//   a[0..7]      at data_mem[0..7]
//   b[0..7]      at data_mem[8..15]
//   result[0..7] at data_mem[16..23]
//
// Addressing note: offsets 0 and 8 fit directly in imm5 (range -16..15),
// so a/b loads use [r7 + offset] directly. Offset 16 does NOT fit --
// so the result address is computed into a register first (IADDI + ADD),
// same technique as exec_test.cpp's STORE test.
// ---------------------------------------------------------------------

int main() {
    std::cout << "--- Vector Add Kernel ---\n\n";

    gpuState state;

    // Seed inputs: a[i] = i+1 (1..8), b[i] = (i+1)*10 (10..80)
    for (size_t i = 0; i < NUM_LANES; ++i) {
        state.data_mem[i]     = static_cast<uint16_t>(i + 1);        // a
        state.data_mem[8 + i] = static_cast<uint16_t>((i + 1) * 10); // b
    }

    int pc = 0;

    // LOAD r2, [r7 + 0]   -- r2 = a[tid]        (format L)
    state.inst_mem[pc++] = (0xA << 12) | (2 << 9) | (7 << 6) | (0 << 1) | 0;

    // LOAD r3, [r7 + 8]   -- r3 = b[tid]        (format L)
    state.inst_mem[pc++] = (0xA << 12) | (3 << 9) | (7 << 6) | (8 << 1) | 0;

    // ADD r4, r2, r3      -- r4 = a[tid] + b[tid]   (format R)
    state.inst_mem[pc++] = (0x1 << 12) | (4 << 9) | (2 << 6) | (3 << 3);

    // IADDI r1, #16       -- r1 = 16 (result base; r1 starts at 0)  (format C)
    state.inst_mem[pc++] = (0x8 << 12) | (1 << 9) | (16 << 1) | 0;

    // ADD r1, r1, r7      -- r1 = 16 + tid (result[tid]'s address)  (format R)
    state.inst_mem[pc++] = (0x1 << 12) | (1 << 9) | (1 << 6) | (7 << 3);

    // STORE [r1 + 0], r4  -- result[tid] = a[tid] + b[tid]          (format S)
    state.inst_mem[pc++] = (0xB << 12) | (1 << 9) | (4 << 6) | (0 << 1) | 0;

    // HALT
    state.inst_mem[pc++] = (0xE << 12);

    run_to_halt(state);

    bool pass = true;
    std::cout << "\nresult[]: ";
    for (size_t i = 0; i < NUM_LANES; ++i) {
        uint16_t a = state.data_mem[i];
        uint16_t b = state.data_mem[8 + i];
        uint16_t expected = a + b;
        uint16_t actual = state.data_mem[16 + i];
        std::cout << actual << " ";
        if (actual != expected) {
            std::cerr << "\n  lane " << i << ": a=" << a << " b=" << b
                       << " expected=" << expected << " got=" << actual << "\n";
            pass = false;
        }
    }
    std::cout << "\n\n";

    print_test_result("result[i] == a[i] + b[i] for all 8 lanes", pass);

    std::cout << "\nVECTOR ADD KERNEL PASSED -- SIMT execution verified end to end.\n";
    return 0;
}