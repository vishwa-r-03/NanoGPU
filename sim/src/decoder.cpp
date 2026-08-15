#include "decoder.h"

uint16_t sign_extend(uint16_t value, uint8_t bits) {
    uint16_t mask = 1 << (bits - 1);
    return (value ^ mask) - mask; // Sign-extend the value
}

Format opcode_format(uint8_t opcode) {
    switch (opcode) {
        case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
            return Format::R;   // ADD SUB AND OR XOR SLT SLTU
        case 0x8: case 0x9:
            return Format::C;   // IADDI LUI
        case 0xA:
            return Format::L;   // LOAD
        case 0xB:
            return Format::S;   // STORE
        case 0xC: case 0xD:
            return Format::Z;   // BZ BNZ
        case 0x0: case 0xE: case 0xF:
        default:
            return Format::N;   // NOP HALT SYNC
    }
}

DecodedInst decode(uint16_t instruction) {
    DecodedInst inst{};
    inst.opcode = (instruction >> 12) & 0xF;
    inst.format = opcode_format(inst.opcode);

    switch (inst.format) {
        case Format::R: {
            inst.rd  = (instruction >> 9) & 0x7;
            inst.rs1 = (instruction >> 6) & 0x7;
            inst.rs2 = (instruction >> 3) & 0x7;
            break;
        }
        case Format::C: {
            inst.rd = (instruction >> 9) & 0x7;
            uint8_t raw_imm8 = (instruction >> 1) & 0xFF;
            inst.imm = raw_imm8; // IADDI/LUI both use this raw, unsigned
            break;
        }
        case Format::L: {
            inst.rd  = (instruction >> 9) & 0x7;
            inst.rs1 = (instruction >> 6) & 0x7;
            uint8_t raw_imm5 = (instruction >> 1) & 0x1F;
            inst.imm5_s = sign_extend(raw_imm5, 5);
            break;
        }
        case Format::S: {
            inst.rs1 = (instruction >> 9) & 0x7;
            inst.rs2 = (instruction >> 6) & 0x7;
            uint8_t raw_imm5 = (instruction >> 1) & 0x1F;
            inst.imm5_s = sign_extend(raw_imm5, 5);
            break;
        }
        case Format::Z: {
            inst.rs1 = (instruction >> 9) & 0x7;
            uint8_t raw_imm8 = (instruction >> 1) & 0xFF;
            inst.imm8_s = sign_extend(raw_imm8, 8);
            break;
        }
        case Format::N:
            // No operand fields -- opcode alone is enough (NOP, HALT, SYNC).
            break;
    }

    return inst;
}