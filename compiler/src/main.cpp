// SPDX-License-Identifier: Apache-2.0
// openclicknp-cc — compiler driver entry point.
#include "openclicknp/driver.hpp"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

namespace {

void usage() {
    std::cerr <<
"openclicknp-cc — clean-room ClickNP DSL compiler\n"
"\n"
"Usage: openclicknp-cc [options] <input.clnp>\n"
"\n"
"Options:\n"
"  -o <dir>             output directory (default: generated/)\n"
"  --platform <p>       target platform: u50_xdma | u50_qdma\n"
"                         (default: u50_xdma)\n"
"  -I <dir>             add directory to import search path\n"
"  --parse-only         parse and exit\n"
"  --no-hls             skip Vitis HLS C++ backend\n"
"  --no-systemc         skip SystemC backend\n"
"  --no-swemu           skip software-emulator backend\n"
"  --no-verilator       skip Verilator harness backend\n"
"  --no-link            skip v++ link config backend\n"
"  --no-host            skip XRT host stub backend\n"
"  -h, --help           print this help\n"
;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 1; }
    openclicknp::DriverOptions opts;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") { usage(); return 0; }
        else if (a == "-o" && i + 1 < argc) opts.output_dir = argv[++i];
        else if (a == "--platform" && i + 1 < argc) {
            opts.platform = openclicknp::be::platformFromString(argv[++i]);
        }
        else if (a == "-I" && i + 1 < argc) opts.import_dirs.emplace_back(argv[++i]);
        else if (a == "--parse-only")    opts.parse_only = true;
        else if (a == "--no-hls")        opts.emit_hls_cpp = false;
        else if (a == "--no-systemc")    opts.emit_systemc = false;
        else if (a == "--no-swemu")      opts.emit_sw_emu = false;
        else if (a == "--no-verilator")  opts.emit_verilator_sim = false;
        else if (a == "--no-link")       opts.emit_vpp_link = false;
        else if (a == "--no-host")       opts.emit_xrt_host = false;
        else if (!a.empty() && a[0] != '-') opts.input_path = a;
        else { std::cerr << "unknown option: " << a << "\n"; usage(); return 1; }
    }
    if (opts.input_path.empty()) { usage(); return 1; }
    openclicknp::Driver d;
    return d.run(opts);
}
