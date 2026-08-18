// regfile.v -- per-lane register file.
//
// Hardware mirror of `lane` in sim/include/state.h. Each of the 8 core
// lanes gets its OWN instance of this module (8 total in the full core) --
// unlike software where one Lane struct lives in an array, in hardware
// each lane is a physically separate copy of this circuit.
//
// Design decisions already settled by looking at what the C++ side needs:
//   - TWO read ports: every ALU op in sim/src/exec.cpp reads at most two
//     source registers (rs1, rs2) in the same instruction, so this module
//     needs to hand back two register values in the same cycle.
//   - LANE_ID as a module PARAMETER, not hardwired: in C++, lane::init()
//     took lane_id as a runtime argument. In hardware there's no
//     equivalent of "call init() later" -- each of the 8 instances needs
//     to know which lane it is from the moment it's instantiated. A
//     Verilog `parameter` is exactly this: a compile-time constant that
//     gets set differently per instance (see the comment near the bottom
//     of this file for what instantiation looks like).
//
// Reset behavior: all registers are cleared to 0, except for register 7 which
// is set to the lane ID (0-7) and is read-only thereafter. This mirrors the C++ lane::init() behavior.
// Synchronous reset is used here since it is more common and avoids potential issues with asynchronous resets in FPGA designs.

module regfile #(
    parameter REG_WIDTH = 16,   // DATA_WIDTH (16-bit words)
    parameter NUM_REGS  = 8,    // REGS_PER_LANE
    parameter LANE_ID   = 0     // this instance's thread ID (0-7), fixed at instantiation
)(
    input  wire                          clk,
    input  wire                          rst,

    // --- Write port ---
    input  wire                          we,     // write enable
    input  wire [$clog2(NUM_REGS)-1:0]   waddr,  // which register to write
    input  wire [REG_WIDTH-1:0]          wdata,  // value to write

    // --- Read ports (x2) ---
    input  wire [$clog2(NUM_REGS)-1:0]   raddr1,
    output wire [REG_WIDTH-1:0]          rdata1,

    input  wire [$clog2(NUM_REGS)-1:0]   raddr2,
    output wire [REG_WIDTH-1:0]          rdata2
);

    // $clog2(NUM_REGS) = $clog2(8) = 3 -- note this matches the 3-bit
    // register address fields from isa.md's instruction encoding. Not a
    // coincidence: the ISA's field width and the hardware's address bus
    // width have to agree, and $clog2 computing it from NUM_REGS (instead
    // of hardcoding "3") means if NUM_REGS ever changes, this updates
    // automatically instead of silently going stale.

    // --- Storage ---
    reg [REG_WIDTH-1:0] regs [0:NUM_REGS-1];  // 8 registers, each 16 bits wide

    // --- Reset and write logic (clocked) ---
    always @(posedge clk) begin
        if (rst) begin
            for (int i = 0; i < NUM_REGS; i++) begin
                regs[i] <= 0;  // reset all registers to 0   
            end
            regs[7] <= LANE_ID;  // set register 7 to the lane ID

        end else if (we) begin
            case (waddr)
                3'b000: regs[0] <= regs[0];  // discard writes to register 0
                3'b111: regs[7] <= regs[7];  // discard writes to register 7
                default: regs[waddr] <= wdata;  // write to other registers
            endcase
        end
    end

    // --- Combinational reads ---
    assign rdata1 = (raddr1 == 0)? 16'h0000 : regs[raddr1];
    assign rdata2 = (raddr2 == 0)? 16'h0000 : regs[raddr2];
    
endmodule

// --- What instantiating 8 of these will look like, later in the core ---
// (Not part of this file -- just here so the LANE_ID parameter's purpose
// is concrete rather than abstract.)
//
//   regfile #(.LANE_ID(0)) lane0_regs (.clk(clk), .rst(rst), ...);
//   regfile #(.LANE_ID(1)) lane1_regs (.clk(clk), .rst(rst), ...);
//   ...
//   regfile #(.LANE_ID(7)) lane7_regs (.clk(clk), .rst(rst), ...);
//
// Eight physically separate circuits, each permanently "knowing" its own
// lane number the moment it's built -- this is the literal hardware.