#include "decoder.h"

uint16_t sign_extend(uint16_t value, uint8_t bits) {
    uint16_t mask = 1 << (bits - 1);
    return (value ^ mask) - mask; // Sign-extend the value
}

format opcode_format(uint8_t opcode) {
    switch (opcode) {
        case 0x1: case 0x2: case 0x3: case 0x4: case 0x5: case 0x6: case 0x7:
            return format::R;   // ADD SUB AND OR XOR SLT SLTU
        case 0x8: case 0x9:
            return format::C;   // IADDI LUI
        case 0xA:
            return format::L;   // LOAD
        case 0xB:
            return format::S;   // STORE
        case 0xC: case 0xD:
            return format::Z;   // BZ BNZ
        case 0x0: case 0xE: case 0xF:
        default:
            return format::N;   // NOP HALT SYNC
    }
}

decodedInst decode(uint16_t instruction) {
    decodedInst inst{};
    inst.opcode = (instruction >> 12) & 0xF;
    inst.fmt = opcode_format(inst.opcode);

    switch (inst.fmt) {
        case format::R: {
            inst.rd  = (instruction >> 9) & 0x7;
            inst.rs1 = (instruction >> 6) & 0x7;
            inst.rs2 = (instruction >> 3) & 0x7;
            break;
        }
        case format::C: {
            inst.rd = (instruction >> 9) & 0x7;
            uint8_t raw_imm8 = (instruction >> 1) & 0xFF;
            inst.imm = raw_imm8; // IADDI/LUI both use this raw, unsigned
            break;
        }
        case format::L: {
            inst.rd  = (instruction >> 9) & 0x7;
            inst.rs1 = (instruction >> 6) & 0x7;
            uint8_t raw_imm5 = (instruction >> 1) & 0x1F;
            inst.imm5_s = sign_extend(raw_imm5, 5);
            break;
        }
        case format::S: {
            inst.rs1 = (instruction >> 9) & 0x7;
            inst.rs2 = (instruction >> 6) & 0x7;
            uint8_t raw_imm5 = (instruction >> 1) & 0x1F;
            inst.imm5_s = sign_extend(raw_imm5, 5);
            break;
        }
        case format::Z: {
            inst.rs1 = (instruction >> 9) & 0x7;
            uint8_t raw_imm8 = (instruction >> 1) & 0xFF;
            inst.imm8_s = sign_extend(raw_imm8, 8);
            break;
        }
        case format::N:
            // No operand fields -- opcode alone is enough (NOP, HALT, SYNC).
            break;
    }

    return inst;
}