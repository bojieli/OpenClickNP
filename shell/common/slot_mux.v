// SPDX-License-Identifier: Apache-2.0
//
// slot_mux — round-robin merger of N per-slot streams onto a single AXIS
// output. The chosen slot's flit is forwarded with its 16-bit slot_id
// already encoded at bytes 48..49 of the flit (set upstream).
`timescale 1ns/1ps
module slot_mux #(
    parameter integer NUM_SLOTS = 32
)(
    input  wire clk,
    input  wire rstn,

    input  wire [NUM_SLOTS*512-1:0] s_axis_tdata,
    input  wire [NUM_SLOTS-1:0]     s_axis_tvalid,
    output wire [NUM_SLOTS-1:0]     s_axis_tready,

    output wire [511:0] m_axis_tdata,
    output wire         m_axis_tvalid,
    input  wire         m_axis_tready
);
    reg  [$clog2(NUM_SLOTS):0] rr;
    integer j;
    reg [NUM_SLOTS-1:0] grant;
    always @* begin
        grant = {NUM_SLOTS{1'b0}};
        for (j = 0; j < NUM_SLOTS; j = j + 1) begin
            // Highest-priority slot = (rr + 1 + j) mod NUM_SLOTS that is valid.
            if (grant == {NUM_SLOTS{1'b0}}) begin
                if (s_axis_tvalid[(rr + 1 + j) % NUM_SLOTS])
                    grant[(rr + 1 + j) % NUM_SLOTS] = 1'b1;
            end
        end
    end
    always @(posedge clk) begin
        if (!rstn) rr <= 0;
        else if (m_axis_tvalid && m_axis_tready) begin
            for (j = 0; j < NUM_SLOTS; j = j + 1) if (grant[j]) rr <= j[$clog2(NUM_SLOTS):0];
        end
    end
    assign m_axis_tvalid = |grant;
    assign s_axis_tready = grant & {NUM_SLOTS{m_axis_tready}};
    integer k;
    reg [511:0] sel_data;
    always @* begin
        sel_data = 512'd0;
        for (k = 0; k < NUM_SLOTS; k = k + 1) if (grant[k]) sel_data = s_axis_tdata[k*512 +: 512];
    end
    assign m_axis_tdata = sel_data;
endmodule
