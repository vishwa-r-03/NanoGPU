#include <iostream>
#include "state.h"
#include "decoder.h"
#include "exec.h"

void print_test_result(const std::string& name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name << "\n";
    if (!passed) std::exit(1);
}

// Run until the machine halts, with a safety cap so a real bug (e.g. an
// infinite loop from a bad branch) can't hang the test suite forever.
void run_to_halt(gpuState& state, int max_cycles = 100) {
    int cycles = 0;
    while (!state.halted && cycles < max_cycles) {
        step(state);
        ++cycles;
    }
    print_test_result("Program halted within cycle budget", state.halted);
}

int main() {
    std::cout << "--- Starting Executor Verification Tests ---\n\n";

    // ------------------------------------------------------------------
    // TEST 1: ADD is broadcast identically to every lane, but each lane
    // computes its own result because r7 (thread ID) differs per lane.
    //
    //   IADDI r1, #10      ; r1 = r1 + 10   -> every lane: r1 = 10
    //   ADD   r2, r1, r7   ; r2 = r1 + r7   -> lane i: r2 = 10 + i
    //   HALT
    // ------------------------------------------------------------------
    {
        gpuState state;
        // IADDI r1, #10  -- format C: opcode(0x8) rd(001) imm8(00001010) P(0)
        state.inst_mem[0] = (0x8 << 12) | (1 << 9) | (10 << 1) | 0;
        // ADD r2, r1, r7 -- format R: opcode(0x1) rd(010) rs1(001) rs2(111)
        state.inst_mem[1] = (0x1 << 12) | (2 << 9) | (1 << 6) | (7 << 3);
        // HALT -- format N: opcode(0xE)
        state.inst_mem[2] = (0xE << 12);

        run_to_halt(state);

        bool pass = true;
        for (size_t i = 0; i < NUM_LANES; ++i) {
            uint16_t expected = 10 + static_cast<uint16_t>(i);
            uint16_t actual = state.lanes[i].read_reg(2);
            if (actual != expected) {
                std::cerr << "  lane " << i << ": expected r2=" << expected
                           << ", got " << actual << "\n";
                pass = false;
            }
        }
        print_test_result("SIMT broadcast: same instructions, per-lane results via r7", pass);
    }

    // ------------------------------------------------------------------
    // TEST 2: LOAD/STORE round-trip, indexed by r7 -- the exact access
    // pattern a real vector-add kernel will use.
    //
    //   Pre-seed data_mem[0..7] = 100..107 by hand before running.
    //   LOAD  r1, [r0 + r7]   -- can't LOAD with a register offset in our
    //                            ISA (offset is an immediate), so instead:
    //   IADDI r1, #100         ; r1 = 100 (same in every lane)
    //   ADD   r2, r1, r7       ; r2 = 100 + i (this lane's target address)
    //   STORE [r2 + 0], r7     ; data_mem[100+i] = i
    //   HALT
    // ------------------------------------------------------------------
    {
        gpuState state;
        // IADDI r1, #100 -- format C
        state.inst_mem[0] = (0x8 << 12) | (1 << 9) | (100 << 1) | 0;
        // ADD r2, r1, r7 -- format R
        state.inst_mem[1] = (0x1 << 12) | (2 << 9) | (1 << 6) | (7 << 3);
        // STORE [r2 + 0], r7 -- format S: opcode(0xB) rs1(010) rs2(111) imm5(00000)
        state.inst_mem[2] = (0xB << 12) | (2 << 9) | (7 << 6) | (0 << 1);
        // HALT
        state.inst_mem[3] = (0xE << 12);

        run_to_halt(state);

        bool pass = true;
        for (size_t i = 0; i < NUM_LANES; ++i) {
            uint16_t expected = static_cast<uint16_t>(i);
            uint16_t actual = state.data_mem[100 + i];
            if (actual != expected) {
                std::cerr << "  data_mem[" << (100 + i) << "]: expected " << expected
                           << ", got " << actual << "\n";
                pass = false;
            }
        }
        print_test_result("STORE writes each lane's own r7 to its own address", pass);
    }

    // ------------------------------------------------------------------
    // TEST 3: HALT actually stops execution -- a second step() call after
    // halting should be a safe no-op, not crash or keep executing.
    // ------------------------------------------------------------------
    {
        gpuState state;
        state.inst_mem[0] = (0xE << 12); // HALT immediately
        step(state);
        bool halted_after_one_step = state.halted;
        step(state); // should be a no-op, not a crash
        bool still_halted = state.halted;
        print_test_result("HALT stops execution and stays stopped", halted_after_one_step && still_halted);
    }

    std::cout << "\nALL EXECUTOR TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}