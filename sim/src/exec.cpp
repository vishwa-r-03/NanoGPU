#include "exec.h"
#include <iostream>

void step(gpuState& state){

    if(state.halted){
        std::cout << "Execution halted. No further steps can be taken." << std::endl;
        return;
    }

    // Fetch and decode (shared across all lanes)
    uint16_t raw_inst = state.inst_mem[state.pc];
    decodedInst inst = decode(raw_inst);

    // track pc modification for branches
    bool pc_modified = false;

    //handle warp-level control instructions before/alongside per lane execution
    switch (inst.opcode){
        case 0xE: // HALT
            state.halted = true;
            std::cout << "HALT instruction executed. Halting execution." << std::endl;
            return; // Exit early since we are halting
        case 0xF: // SYNC
            std::cout << "SYNC instruction executed. All lanes synchronized." << std::endl;
            break;
        case 0x0: // NOP
            break;
        case 0xC: { // BZ
            uint16_t val = state.lanes[0].read_reg(inst.rs1);
            if(val == 0){
                state.pc += inst.imm8_s; // Branch taken
                pc_modified = true;
            }
            break;
        }
        case 0xD: { // BNZ
            uint16_t val = state.lanes[0].read_reg(inst.rs1);
            if(val != 0){
                state.pc += inst.imm8_s; // Branch taken
                pc_modified = true;
            }
            break;
        }
        default:
            break; // No special handling for other opcodes at the warp level
    }

    // SIMT execution loop (per lane)
    for (size_t lane_id = 0; lane_id < NUM_LANES; lane_id++){
        lane& ln = state.lanes[lane_id];

        switch (inst.opcode){
            case 0x0:
            case 0xC:
            case 0xD:
            case 0xE:
            case 0xF:
                // These opcodes are handled at the warp level, so we skip them here.
                break;
            case 0x1: // ADD
                ln.write_reg(inst.rd, ln.read_reg(inst.rs1) + ln.read_reg(inst.rs2));
                break;
            case 0x2: // SUB
                ln.write_reg(inst.rd, ln.read_reg(inst.rs1) - ln.read_reg(inst.rs2));
                break;
            case 0x3: // AND
                ln.write_reg(inst.rd, ln.read_reg(inst.rs1) & ln.read_reg(inst.rs2));
                break;
            case 0x4: // OR
                ln.write_reg(inst.rd, ln.read_reg(inst.rs1) | ln.read_reg(inst.rs2));
                break;
            case 0x5: // XOR
                ln.write_reg(inst.rd, ln.read_reg(inst.rs1) ^ ln.read_reg(inst.rs2));
                break;
            case 0x6: // SLT
                ln.write_reg(inst.rd, ((int16_t)ln.read_reg(inst.rs1) < (int16_t)ln.read_reg(inst.rs2) ? 1 : 0));
                break;
            case 0x7: // SLTU
                ln.write_reg(inst.rd, (ln.read_reg(inst.rs1) < ln.read_reg(inst.rs2) ? 1 : 0));
                break;
            case 0x8: // IADDI
                ln.write_reg(inst.rd, ln.read_reg(inst.rd) + inst.imm);
                break;
            case 0x9: // LUI
                ln.write_reg(inst.rd, inst.imm << 8);
                break;
            case 0xA: { // LOAD
                uint16_t addr = ln.read_reg(inst.rs1) + inst.imm5_s;
                if(addr < DATA_MEM_SIZE){
                    ln.write_reg(inst.rd, state.data_mem[addr]);
                } else {
                    std::cerr << "[lane " << lane_id << "] LOAD out of bounds: addr="
                              << addr << " (pc=" << state.pc << ")" << std::endl;
                    state.halted = true;
                    return;
                }
                break;
            }
            case 0xB: { // STORE
                uint16_t addr = ln.read_reg(inst.rs1) + inst.imm5_s;
                if(addr < DATA_MEM_SIZE){
                    state.data_mem[addr] = ln.read_reg(inst.rs2);
                } else {
                    std::cerr << "[lane " << lane_id << "] STORE out of bounds: addr="
                              << addr << " (pc=" << state.pc << ")" << std::endl;
                    state.halted = true;
                    return;
                }
                break;
            }
            default:
                std::cerr << "Unknown opcode: " << std::hex << static_cast<int>(inst.opcode) << std::dec << std::endl;
                break;
        }
    }

    // if the PC was not modified by a branch, increment it to point to the next instruction
    if(!pc_modified){
        state.pc++;
    }
}