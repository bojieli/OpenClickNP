// SPDX-License-Identifier: Apache-2.0
//
// openclicknp_status — minimal AXI-Lite slave exposing:
//   0x00 cycle_lo
//   0x04 cycle_hi
//   0x08 tor_rx_cnt   (incremented on tor_in EOP handshake)
//   0x0C tor_tx_cnt
//   0x10 nic_rx_cnt
//   0x14 nic_tx_cnt
//
// 12-bit address space (one 4 KiB page).
`timescale 1ns/1ps
module openclicknp_status (
    input  wire ap_clk,
    input  wire ap_rst_n,

    input  wire tor_rx_pkt,
    input  wire tor_tx_pkt,
    input  wire nic_rx_pkt,
    input  wire nic_tx_pkt,

    input  wire [11:0] s_axil_awaddr,
    input  wire        s_axil_awvalid,
    output reg         s_axil_awready,
    input  wire [31:0] s_axil_wdata,
    input  wire [3:0]  s_axil_wstrb,
    input  wire        s_axil_wvalid,
    output reg         s_axil_wready,
    output reg [1:0]   s_axil_bresp,
    output reg         s_axil_bvalid,
    input  wire        s_axil_bready,
    input  wire [11:0] s_axil_araddr,
    input  wire        s_axil_arvalid,
    output reg         s_axil_arready,
    output reg [31:0]  s_axil_rdata,
    output reg [1:0]   s_axil_rresp,
    output reg         s_axil_rvalid,
    input  wire        s_axil_rready
);
    reg [63:0] cycles;
    reg [31:0] tor_rx, tor_tx, nic_rx, nic_tx;

    always @(posedge ap_clk) begin
        if (!ap_rst_n) begin
            cycles  <= 64'd0;
            tor_rx  <= 32'd0;
            tor_tx  <= 32'd0;
            nic_rx  <= 32'd0;
            nic_tx  <= 32'd0;
        end else begin
            cycles  <= cycles + 64'd1;
            if (tor_rx_pkt) tor_rx <= tor_rx + 1;
            if (tor_tx_pkt) tor_tx <= tor_tx + 1;
            if (nic_rx_pkt) nic_rx <= nic_rx + 1;
            if (nic_tx_pkt) nic_tx <= nic_tx + 1;
        end
    end

    // AXI-Lite read path.
    always @(posedge ap_clk) begin
        if (!ap_rst_n) begin
            s_axil_arready <= 1'b0;
            s_axil_rvalid  <= 1'b0;
            s_axil_rresp   <= 2'b00;
            s_axil_rdata   <= 32'd0;
        end else begin
            if (s_axil_arvalid && !s_axil_arready) begin
                s_axil_arready <= 1'b1;
                case (s_axil_araddr[7:0])
                    8'h00: s_axil_rdata <= cycles[31:0];
                    8'h04: s_axil_rdata <= cycles[63:32];
                    8'h08: s_axil_rdata <= tor_rx;
                    8'h0C: s_axil_rdata <= tor_tx;
                    8'h10: s_axil_rdata <= nic_rx;
                    8'h14: s_axil_rdata <= nic_tx;
                    default: s_axil_rdata <= 32'd0;
                endcase
                s_axil_rvalid <= 1'b1;
            end else if (s_axil_rvalid && s_axil_rready) begin
                s_axil_rvalid  <= 1'b0;
                s_axil_arready <= 1'b0;
            end
        end
    end

    // AXI-Lite write path: writes are silently consumed (status block is RO
    // for telemetry; non-status cmd channels live in per-kernel signal IPs).
    always @(posedge ap_clk) begin
        if (!ap_rst_n) begin
            s_axil_awready <= 1'b0;
            s_axil_wready  <= 1'b0;
            s_axil_bvalid  <= 1'b0;
            s_axil_bresp   <= 2'b00;
        end else begin
            s_axil_awready <= s_axil_awvalid && !s_axil_awready;
            s_axil_wready  <= s_axil_wvalid  && !s_axil_wready;
            if (s_axil_awvalid && s_axil_wvalid && !s_axil_bvalid) begin
                s_axil_bvalid <= 1'b1;
            end else if (s_axil_bvalid && s_axil_bready) begin
                s_axil_bvalid <= 1'b0;
            end
        end
    end
endmodule
