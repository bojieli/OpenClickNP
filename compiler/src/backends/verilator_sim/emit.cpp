// SPDX-License-Identifier: Apache-2.0
// Verilator full-system simulation harness emitter.
//
// Produces:
//   generated/verilator/topology.v   -- top-level Verilog instantiation
//   generated/verilator/tb.cpp       -- C++ Verilator testbench
//
// Assumes the per-kernel Verilog from Vitis HLS lives at
// build/kernels/<name>.v and follows AXIS naming conventions.

#include "openclicknp/passes.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace openclicknp::backends {

namespace {

void emitTopVerilog(std::ostream& os, const be::Build& b) {
    os << "// Auto-generated Verilog top for Verilator full-system sim\n";
    os << "module openclicknp_sim_top (\n"
       << "    input  wire        clk,\n"
       << "    input  wire        rstn,\n"
       << "    // tor boundary AXIS\n"
       << "    input  wire [511:0] tor_in_tdata,\n"
       << "    input  wire         tor_in_tvalid,\n"
       << "    output wire         tor_in_tready,\n"
       << "    output wire [511:0] tor_out_tdata,\n"
       << "    output wire         tor_out_tvalid,\n"
       << "    input  wire         tor_out_tready,\n"
       << "    // nic boundary AXIS\n"
       << "    input  wire [511:0] nic_in_tdata,\n"
       << "    input  wire         nic_in_tvalid,\n"
       << "    output wire         nic_in_tready,\n"
       << "    output wire [511:0] nic_out_tdata,\n"
       << "    output wire         nic_out_tvalid,\n"
       << "    input  wire         nic_out_tready\n"
       << ");\n\n";

    // Per-edge AXIS wires.
    int eid = 0;
    for (const auto& sc : b.stream_conns) {
        os << "    wire [511:0] e" << eid << "_tdata;\n";
        os << "    wire         e" << eid << "_tvalid;\n";
        os << "    wire         e" << eid << "_tready;\n";
        ++eid;
    }
    os << "\n    // Kernel instantiations (placeholders — synthesized RTL\n"
       << "    // is provided by Vitis HLS).\n";
    for (const auto& k : b.kernels) {
        os << "    " << k.name << " u_" << k.name << " (\n"
           << "        .ap_clk(clk), .ap_rst_n(rstn)\n"
           << "        // ports wired by build script post-HLS\n"
           << "    );\n";
    }
    os << "\nendmodule\n";
}

void emitVerilatorCpp(std::ostream& os, const be::Build& b) {
    os << "// SPDX-License-Identifier: Apache-2.0\n"
       << "// Verilator testbench for openclicknp\n"
       << "#include <verilated.h>\n"
       << "#include <verilated_fst_c.h>\n"
       << "#include \"Vopenclicknp_sim_top.h\"\n"
       << "#include <cstdio>\n\n";
    os << "int main(int argc, char** argv) {\n"
       << "    Verilated::commandArgs(argc, argv);\n"
       << "    Verilated::traceEverOn(true);\n"
       << "    auto* top = new Vopenclicknp_sim_top;\n"
       << "    auto* trace = new VerilatedFstC;\n"
       << "    top->trace(trace, 99);\n"
       << "    trace->open(\"topology.fst\");\n"
       << "    top->rstn = 0; top->clk = 0;\n"
       << "    for (int t = 0; t < 200000; ++t) {\n"
       << "        top->clk = !top->clk;\n"
       << "        if (t == 20) top->rstn = 1;\n"
       << "        top->eval();\n"
       << "        trace->dump(t);\n"
       << "    }\n"
       << "    trace->close();\n"
       << "    delete trace; delete top;\n"
       << "    return 0;\n}\n";
    (void)b;
}

}  // namespace

bool emitVerilator(const be::Build& build, const std::string& outdir,
                   DiagnosticEngine& d) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(outdir) / "verilator";
    fs::create_directories(dir);
    {
        std::ofstream f(dir / "topology.v");
        if (!f) { d.error({}, "cannot write topology.v"); return false; }
        emitTopVerilog(f, build);
    }
    {
        std::ofstream f(dir / "tb.cpp");
        if (!f) { d.error({}, "cannot write tb.cpp"); return false; }
        emitVerilatorCpp(f, build);
    }
    return true;
}

}  // namespace openclicknp::backends
