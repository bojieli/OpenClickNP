// SPDX-License-Identifier: Apache-2.0
//
// OpenClickNP top-level wrapper for the U50/XDMA platform.
//
// This module sits between:
//   - the XDMA shell (PCIe → AXIS H2C/C2H + AXI-Lite control)
//   - two CMAC 100G subsystems (tor: QSFP28 cage A, nic: QSFP28 cage B)
//   - the HBM controller (8 GB; 32 pseudo-channels via AXI smartconnect)
//   - the per-kernel user logic synthesized by Vitis HLS via v++.
//
// All user-side AXIS runs at the same 322.265625 MHz clock (`ap_clk`).
// CDC into CMAC TX/RX clocks and HBM AXI clocks happens inside the
// respective vendor IP blocks; clocks.xdc declares those as async groups.

`timescale 1ns/1ps
module openclicknp_top #(
    parameter integer NUM_KERNEL_AXIS = 32
)(
    input  wire        ap_clk,
    input  wire        ap_rst_n,

    // ToR-side CMAC AXIS to user kernels (RX) and from user kernels (TX).
    output wire [511:0] tor_in_tdata,
    output wire         tor_in_tvalid,
    input  wire         tor_in_tready,
    output wire [63:0]  tor_in_tkeep,
    output wire         tor_in_tlast,

    input  wire [511:0] tor_out_tdata,
    input  wire         tor_out_tvalid,
    output wire         tor_out_tready,
    input  wire [63:0]  tor_out_tkeep,
    input  wire         tor_out_tlast,

    // NIC-side CMAC AXIS.
    output wire [511:0] nic_in_tdata,
    output wire         nic_in_tvalid,
    input  wire         nic_in_tready,
    output wire [63:0]  nic_in_tkeep,
    output wire         nic_in_tlast,

    input  wire [511:0] nic_out_tdata,
    input  wire         nic_out_tvalid,
    output wire         nic_out_tready,
    input  wire [63:0]  nic_out_tkeep,
    input  wire         nic_out_tlast,

    // Host streaming interface (slot-multiplexed XDMA H2C/C2H).
    input  wire [511:0] host_h2c_tdata,
    input  wire         host_h2c_tvalid,
    output wire         host_h2c_tready,
    output wire [511:0] host_c2h_tdata,
    output wire         host_c2h_tvalid,
    input  wire         host_c2h_tready,

    // AXI-Lite for the openclicknp_status block (cycle/network counters).
    input  wire [11:0]  s_axil_awaddr,
    input  wire         s_axil_awvalid,
    output wire         s_axil_awready,
    input  wire [31:0]  s_axil_wdata,
    input  wire [3:0]   s_axil_wstrb,
    input  wire         s_axil_wvalid,
    output wire         s_axil_wready,
    output wire [1:0]   s_axil_bresp,
    output wire         s_axil_bvalid,
    input  wire         s_axil_bready,
    input  wire [11:0]  s_axil_araddr,
    input  wire         s_axil_arvalid,
    output wire         s_axil_arready,
    output wire [31:0]  s_axil_rdata,
    output wire [1:0]   s_axil_rresp,
    output wire         s_axil_rvalid,
    input  wire         s_axil_rready
);

    // Status / counters block (cycle counter + CMAC packet counters).
    openclicknp_status u_status (
        .ap_clk      (ap_clk),
        .ap_rst_n    (ap_rst_n),

        .tor_rx_pkt  (tor_in_tvalid  & tor_in_tready  & tor_in_tlast),
        .tor_tx_pkt  (tor_out_tvalid & tor_out_tready & tor_out_tlast),
        .nic_rx_pkt  (nic_in_tvalid  & nic_in_tready  & nic_in_tlast),
        .nic_tx_pkt  (nic_out_tvalid & nic_out_tready & nic_out_tlast),

        .s_axil_awaddr  (s_axil_awaddr),
        .s_axil_awvalid (s_axil_awvalid),
        .s_axil_awready (s_axil_awready),
        .s_axil_wdata   (s_axil_wdata),
        .s_axil_wstrb   (s_axil_wstrb),
        .s_axil_wvalid  (s_axil_wvalid),
        .s_axil_wready  (s_axil_wready),
        .s_axil_bresp   (s_axil_bresp),
        .s_axil_bvalid  (s_axil_bvalid),
        .s_axil_bready  (s_axil_bready),
        .s_axil_araddr  (s_axil_araddr),
        .s_axil_arvalid (s_axil_arvalid),
        .s_axil_arready (s_axil_arready),
        .s_axil_rdata   (s_axil_rdata),
        .s_axil_rresp   (s_axil_rresp),
        .s_axil_rvalid  (s_axil_rvalid),
        .s_axil_rready  (s_axil_rready)
    );

    // The actual user kernel graph is instantiated by v++ via the
    // generated/connectivity.cfg stream_connect entries; this top file
    // exposes the boundary AXIS ports the v++ linker wires up.

endmodule
