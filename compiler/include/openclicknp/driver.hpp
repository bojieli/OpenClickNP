// SPDX-License-Identifier: Apache-2.0
// Top-level compiler driver: orchestrates parse → resolve → analyses → emit.
#pragma once

#include "openclicknp/be_ir.hpp"
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/eg_ir.hpp"
#include "openclicknp/source.hpp"

#include <string>
#include <vector>

namespace openclicknp {

struct DriverOptions {
    std::string input_path;
    std::string output_dir = "generated";
    be::Platform platform = be::Platform::U50_XDMA;

    // Backend toggles
    bool emit_hls_cpp       = true;
    bool emit_systemc       = true;
    bool emit_sw_emu        = true;
    bool emit_verilator_sim = true;
    bool emit_vpp_link      = true;
    bool emit_xrt_host      = true;

    // Search path for .clnp imports.
    std::vector<std::string> import_dirs;

    bool parse_only = false;
    bool dump_eg_ir = false;
    bool dump_be_ir = false;
};

class Driver {
public:
    // Returns the number of errors emitted (0 == success).
    int run(const DriverOptions& opts);
};

}  // namespace openclicknp
