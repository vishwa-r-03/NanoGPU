#include "state.h"

// --- Lane Implementation ---

Lane::Lane() {
    regs.fill(0);
}

void Lane::init(uint16_t lane_id) {
    regs.fill(0);
    regs[0] = 0;        // r0 is hardwired to 0
    regs[7] = lane_id;  // r7 is per-lane thread index (0-7)
}

// Read register (r0 always returns 0)
uint16_t Lane::read_reg(size_t reg_num) const {
    if (reg_num >= REGS_PER_LANE || reg_num == 0) {
        return 0; // Out of bounds or r0 reads as 0
    }
    return regs[reg_num];
}

// Write register (r0 and r7 are read-only)
void Lane::write_reg(size_t reg_num, uint16_t value) {
    // Early return for invalid or protected registers
    if (reg_num >= REGS_PER_LANE || reg_num == 0 || reg_num == 7) return; // Discard writes to r0/r7
    regs[reg_num] = value;
}

// --- GPUState Implementation ---

GPUState::GPUState() : pc(0), halted(false) {
    reset();
}

void GPUState::reset() {
    pc = 0;
    halted = false;
    data_mem.fill(0);
    inst_mem.fill(0);
    for (uint16_t i = 0; i < NUM_LANES; ++i) {
        lanes[i].init(i);
    }
}