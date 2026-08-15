#include <iostream>
#include <cassert>
#include <iomanip>
#include "decoder.h"

// Helper function to print pass/fail results nicely
void print_test_result(const std::string& name, bool passed) {
    std::cout << "[" << (passed ? "PASS" : "FAIL") << "] " << name << "\n";
    if (!passed) {
        std::exit(1); // Stop execution immediately on test failure
    }
}

int main() {
    std::cout << "--- Starting Decoder Verification Tests ---\n\n";

    // ------------------------------------------------------------------------
    // TEST 1: ADD r1, r2, r3 (Format::R)
    // Opcode: 0x1, rd: 1 (001), rs1: 2 (010), rs2: 3 (011), reserved: 00, P: 0
    // Binary: 0001 0010 1001 1000 -> Hex: 0x1298
    // ------------------------------------------------------------------------
    {
        uint16_t raw_inst = 0x1298;
        DecodedInst d = decode(raw_inst);

        bool pass = (d.opcode == 0x1) &&
                    (d.format == Format::R) &&
                    (d.rd == 1) &&
                    (d.rs1 == 2) &&
                    (d.rs2 == 3);

        print_test_result("ADD r1, r2, r3 (Format R)", pass);
    }

    // ------------------------------------------------------------------------
    // TEST 2: LOAD r4, [r1 + (-2)] (Format::L)
    // Opcode: 0xA, rd: 4 (100), rs1: 1 (001), imm5: -2 (11110 binary), P: 0
    // Layout: opcode[4] · rd[3] · rs1[3] · imm[5] · P[1]
    // Binary: 1010 1000 0111 1100 -> Hex: 0xA87C
    // ------------------------------------------------------------------------
    {
        uint16_t raw_inst = 0xA87C;
        DecodedInst d = decode(raw_inst);

        bool pass = (d.opcode == 0xA) &&
                    (d.format == Format::L) &&
                    (d.rd == 4) &&
                    (d.rs1 == 1) &&
                    ((int16_t)d.imm5_s == -2);

        print_test_result("LOAD r4, [r1 + (-2)] (Format L with signed offset)", pass);
    }

    // ------------------------------------------------------------------------
    // TEST 3: STORE [r5 + 3], r6 (Format::S)
    // Opcode: 0xB, rs1: 5 (101), rs2: 6 (110), imm5: +3 (00011 binary), P: 0
    // Layout: opcode[4] · rs1[3] · rs2[3] · imm[5] · P[1]
    // Binary: 1011 1011 1000 0110 -> Hex: 0xBB86
    // ------------------------------------------------------------------------
    {
        uint16_t raw_inst = 0xBB86;
        DecodedInst d = decode(raw_inst);

        bool pass = (d.opcode == 0xB) &&
                    (d.format == Format::S) &&
                    (d.rs1 == 5) &&
                    (d.rs2 == 6) &&
                    ((int16_t)d.imm5_s == 3);

        print_test_result("STORE [r5 + 3], r6 (Format S)", pass);
    }

    // ------------------------------------------------------------------------
    // TEST 4: BZ r3, -10 (Format::Z)
    // Opcode: 0xC, rs1: 3 (011), imm8: -10 (0xF6 = 11110110 binary), P: 0
    // Layout: opcode[4] · rs1[3] · imm8[8] · P[1]
    // Binary: 1100 0111 1110 1100 -> Hex: 0xC7EC
    // ------------------------------------------------------------------------
    {
        uint16_t raw_inst = 0xC7EC;
        DecodedInst d = decode(raw_inst);

        bool pass = (d.opcode == 0xC) &&
                    (d.format == Format::Z) &&
                    (d.rs1 == 3) &&
                    ((int16_t)d.imm8_s == -10);

        print_test_result("BZ r3, -10 (Format Z with signed negative branch offset)", pass);
    }

    std::cout << "\nALL DECODER TESTS PASSED SUCCESSFULLY!\n";
    return 0;
}