// regfile_tb.v -- testbench for rtl/regfile.v
//
// Run with:
//   iverilog -g2012 -o regfile_tb.vvp rtl/regfile.v tb/regfile_tb.v
//   vvp regfile_tb.vvp
//   gtkwave regfile_tb.vcd     (to inspect the waveform visually)

`timescale 1ns/1ps

module regfile_tb;

    localparam REG_WIDTH = 16;
    localparam NUM_REGS  = 8;
    localparam LANE_ID = 3;

    reg                          clk;
    reg                          rst;
    reg                          we;
    reg  [$clog2(NUM_REGS)-1:0]  waddr;
    reg  [REG_WIDTH-1:0]         wdata;
    reg  [$clog2(NUM_REGS)-1:0]  raddr1;
    wire [REG_WIDTH-1:0]         rdata1;
    reg  [$clog2(NUM_REGS)-1:0]  raddr2;
    wire [REG_WIDTH-1:0]         rdata2;

    integer errors = 0;

    regfile #(
        .REG_WIDTH(REG_WIDTH),
        .NUM_REGS(NUM_REGS),
        .LANE_ID(LANE_ID)
    ) dut (
        .clk(clk), .rst(rst),
        .we(we), .waddr(waddr), .wdata(wdata),
        .raddr1(raddr1), .rdata1(rdata1),
        .raddr2(raddr2), .rdata2(rdata2)
    );

    always #5 clk = ~clk;

    task check(input [8*40:0] name, input [REG_WIDTH-1:0] actual, input [REG_WIDTH-1:0] expected);
        begin
            if (actual !== expected) begin
                $display("[FAIL] %0s -- expected %0d, got %0d", name, expected, actual);
                errors = errors + 1;
            end else begin
                $display("[PASS] %0s", name);
            end
        end
    endtask

    initial begin
        $dumpfile("regfile_tb.vcd");
        $dumpvars(0, regfile_tb);

        clk = 0;
        rst = 0;
        we = 0;
        waddr = 0; wdata = 0;
        raddr1 = 0; raddr2 = 0;

        // 1. Apply reset
        rst = 1;
        @(posedge clk);
        #1;             // settle past the same-edge race window before changing rst
        rst = 0;
        #1;

        // 2. Check reset behavior
        raddr1 = 7;
        raddr2 = 0;
        #1;
        check("r7 reads LANE_ID after reset", rdata1, LANE_ID);
        check("r0 reads 0 after reset", rdata2, 0);

        // 3. Write to a normal register, then read it back
        we = 1;
        waddr = 2;
        wdata = 16'hABCD;
        @(posedge clk);
        #1;
        we = 0;
        raddr1 = 2;
        #1;
        check("write then read back r2", rdata1, 16'hABCD);

        // 4. Attempt to write to r0 -- should be discarded
        we = 1;
        waddr = 0;
        wdata = 16'hFFFF;
        @(posedge clk);
        #1;
        we = 0;
        raddr1 = 0;
        #1;
        check("write to r0 is discarded", rdata1, 16'h0000);

        // 5. Attempt to write to r7 -- should be discarded
        we = 1;
        waddr = 7;
        wdata = 16'h1234;
        @(posedge clk);
        #1;
        we = 0;
        raddr1 = 7;
        #1;
        check("write to r7 is discarded", rdata1, LANE_ID);

        // 6. Simultaneous dual-port read
        we = 1;
        waddr = 3;
        wdata = 16'h1111;
        @(posedge clk);
        #1;
        waddr = 4;
        wdata = 16'h2222;
        @(posedge clk);
        #1;
        we = 0;
        raddr1 = 3;
        raddr2 = 4;
        #1;
        check("dual read port 1 (r3)", rdata1, 16'h1111);
        check("dual read port 2 (r4)", rdata2, 16'h2222);

        if (errors == 0)
            $display("\nALL REGFILE TESTS PASSED SUCCESSFULLY!");
        else
            $display("\n%0d CHECK(S) FAILED.", errors);

        $finish;
    end

endmodule