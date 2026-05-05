// SPDX-License-Identifier: Apache-2.0
//
// slot_demux — fans a single AXIS stream out to N per-slot output streams,
// using the 16-bit slot_id field at byte offsets 48..49 of every flit.
//
// Round-robin selection between slots that have backpressure-free outputs.
`timescale 1ns/1ps
module slot_demux #(
    parameter integer NUM_SLOTS = 32
)(
    input  wire        clk,
    input  wire        rstn,

    input  wire [511:0] s_axis_tdata,
    input  wire         s_axis_tvalid,
    output wire         s_axis_tready,

    output wire [NUM_SLOTS*512-1:0] m_axis_tdata,
    output wire [NUM_SLOTS-1:0]     m_axis_tvalid,
    input  wire [NUM_SLOTS-1:0]     m_axis_tready
);
    wire [15:0] slot = s_axis_tdata[48*8 +: 16];
    reg  [NUM_SLOTS-1:0] sel;
    integer i;
    always @* begin
        sel = {NUM_SLOTS{1'b0}};
        if (slot < NUM_SLOTS) sel[slot] = 1'b1;
    end
    assign s_axis_tready = |(sel & m_axis_tready);
    genvar g;
    generate
        for (g = 0; g < NUM_SLOTS; g = g + 1) begin : per_slot
            assign m_axis_tdata[g*512 +: 512] = s_axis_tdata;
            assign m_axis_tvalid[g]           = s_axis_tvalid & sel[g];
        end
    endgenerate
endmodule
