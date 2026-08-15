#pragma once

#include <cstdint>
#include <array>
#include <cstddef>

// Architectural Constants
constexpr size_t NUM_LANES = 8;        // 8 SIMT execution lanes
constexpr size_t REGS_PER_LANE = 8;    // r0 to r7
constexpr size_t DATA_MEM_SIZE = 512;  // Shared data memory (16-bit words)
constexpr size_t INST_MEM_SIZE = 256;  // Instruction memory

// Single Lane Representation
struct Lane {
    std::array<uint16_t, REGS_PER_LANE> regs{}; // Private register file

    Lane();

    void init(uint16_t lane_id);
    uint16_t read_reg(size_t reg_num) const;
    void write_reg(size_t reg_num, uint16_t value);
};

// Machine State Representation
struct GPUState {
    uint16_t pc;                                   // Program counter (word address)
    bool halted;                                   // Execution completion flag
    
    std::array<Lane, NUM_LANES> lanes;             // 8 SIMT lanes
    std::array<uint16_t, DATA_MEM_SIZE> data_mem;  // Flat unified data memory
    std::array<uint16_t, INST_MEM_SIZE> inst_mem;  // Instruction memory

    GPUState();

    void reset();
};

