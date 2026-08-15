#pragma once

#include <cstdint>

// Every opcode maps to exactly one of these six field layouts.
// See docs/isa.md's "Instruction encoding" table for the bit diagrams.
enum class Format {
    R,  // opcode[4] rd[3] rs1[3] rs2[3] reserved[2] P[1]
    C,  // opcode[4] rd[3] imm[8] P[1]                       -- IADDI, LUI
    L,  // opcode[4] rd[3] rs1[3] imm[5] P[1]                -- LOAD
    S,  // opcode[4] rs1[3] rs2[3] imm[5] P[1]               -- STORE
    Z,  // opcode[4] rs1[3] imm[8] P[1]                      -- BZ, BNZ
    N   // opcode[4] unused[11] P[1]                         -- NOP, HALT, SYNC
};

// Which format a given 4-bit opcode uses. One place, one source of truth --
// decode() below just asks this instead of re-deriving it per opcode.
Format opcode_format(uint8_t opcode);

struct DecodedInst {
    uint8_t opcode;
    Format  format;

    uint8_t rd;
    uint8_t rs1;
    uint8_t rs2;

    uint16_t imm;     // raw/zero-extended immediate (IADDI, LUI)
    uint16_t imm5_s;  // sign-extended 5-bit immediate (LOAD, STORE)
    uint16_t imm8_s;  // sign-extended 8-bit immediate (BZ, BNZ)
};

uint16_t sign_extend(uint16_t value, uint8_t bits);
DecodedInst decode(uint16_t instruction);