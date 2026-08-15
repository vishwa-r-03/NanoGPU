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
            // For this simple simulator, we can just print a message.
            std::cout << "SYNC instruction executed. All lanes synchronized." << std::endl;
            break;
        case 0x0: // NOP
            break;   // No operation, just proceed to the next instruction.
        case 0xC: // BZ
            {uint16_t val = state.lanes[0].read_reg(inst.rs1);
            if(val == 0){
                state.pc += inst.imm8_s; // Branch taken
                pc_modified = true;
            }}
            break;
        case 0xD: // BNZ
            {uint16_t val = state.lanes[0].read_reg(inst.rs1);
            if(val != 0){
                state.pc += inst.imm8_s; // Branch taken
                pc_modified = true;
            }}
            break;
        default:
            break; // No special handling for other opcodes at the warp level
    }

    // SIMT execution loop (per lane)
    for (size_t lane_id = 0; lane_id < NUM_LANES; lane_id++){
        lane& lane = state.lanes[lane_id];

        switch (inst.opcode){
            case 0x0:
            case 0xC:
            case 0xD:
            case 0xE:
            case 0xF:
                // These opcodes are handled at the warp level, so we skip them here.
                break;
            case 0x1: // ADD
                lane.write_reg(inst.rd, lane.read_reg(inst.rs1) + lane.read_reg(inst.rs2));
                break;
            case 0x2: // SUB
                lane.write_reg(inst.rd, lane.read_reg(inst.rs1) - lane.read_reg(inst.rs2));
                break;
            case 0x3: // AND
                lane.write_reg(inst.rd, lane.read_reg(inst.rs1) & lane.read_reg(inst.rs2));
                break;
            case 0x4: // OR
                lane.write_reg(inst.rd, lane.read_reg(inst.rs1) | lane.read_reg(inst.rs2));
                break;
            case 0x5: // XOR
                lane.write_reg(inst.rd, lane.read_reg(inst.rs1) ^ lane.read_reg(inst.rs2));
                break;
            case 0x6: // SLT
                lane.write_reg(inst.rd, ((int16_t)lane.read_reg(inst.rs1) < (int16_t)lane.read_reg(inst.rs2) ? 1 : 0));
                break;  
            case 0x7: // SLTU
                lane.write_reg(inst.rd, (lane.read_reg(inst.rs1) < lane.read_reg(inst.rs2) ? 1 : 0));
                break;
            case 0x8: // IADDI
                lane.write_reg(inst.rd, lane.read_reg(inst.rd) + inst.imm);
                break;
            case 0x9: // LUI
                lane.write_reg(inst.rd, inst.imm << 8);
                break;
            case 0xA: // LOAD
                {uint16_t addr = lane.read_reg(inst.rs1) + inst.imm5_s;
                if(addr < DATA_MEM_SIZE){
                    lane.write_reg(inst.rd, state.data_mem[addr]);
                } else {
                    std::cerr << "LOAD address out of bounds: addr= " << addr <<"(pc=" << state.pc << ")" << std::endl;
                    state.halted = true; // Halt execution on error
                    return; // Exit early since we are halting
                }}
                break;
            case 0xB: // STORE
                {uint16_t addr = lane.read_reg(inst.rs1) + inst.imm5_s;
                if(addr < DATA_MEM_SIZE){
                    state.data_mem[addr] = lane.read_reg(inst.rs2);
                } else {
                    std::cerr << "STORE address out of bounds: addr= " << addr <<"(pc=" << state.pc << ")" << std::endl;
                    state.halted = true; // Halt execution on error
                    return; // Exit early since we are halting
                }}
                break;
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

