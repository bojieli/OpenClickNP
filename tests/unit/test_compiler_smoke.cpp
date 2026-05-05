// SPDX-License-Identifier: Apache-2.0
// End-to-end smoke test: drive Driver::run on a minimal .clnp and check
// that all backends produced their expected files.
#include "openclicknp/driver.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace openclicknp;

static const char kClnp[] = R"(
.element Pass <1,1> {
    .state { uint64_t flits; }
    .init  { _state.flits = 0; }
    .handler {
        if (test_input_port(PORT_1)) {
            openclicknp::flit_t f = read_input_port(PORT_1);
            set_output_port(1, f);
            _state.flits++;
        }
        return PORT_1;
    }
    .signal (uint cmd, uint p) {
        outevent.lparam[0] = _state.flits;
    }
}

Pass :: rx @
tor_in -> rx -> nic_out
)";

int main() {
    namespace fs = std::filesystem;
    // Per-PID scratch dir so this test is safe when ctest -j is used.
    auto pid = std::to_string(::getpid());
    const std::string in_path  = "/tmp/openclicknp_smoke_" + pid + ".clnp";
    const std::string out_path = "/tmp/openclicknp_smoke_gen_" + pid;
    { std::ofstream f(in_path); f << kClnp; }
    fs::remove_all(out_path);

    DriverOptions opts;
    opts.input_path = in_path;
    opts.output_dir = out_path;
    Driver d;
    int rc = d.run(opts);
    assert(rc == 0);

    assert(fs::exists(out_path + "/kernels/rx.cpp"));
    assert(fs::exists(out_path + "/kernels/rx_tb.cpp"));
    assert(fs::exists(out_path + "/sw_emu/topology.cpp"));
    assert(fs::exists(out_path + "/systemc/topology.cpp"));
    assert(fs::exists(out_path + "/verilator/topology.v"));
    assert(fs::exists(out_path + "/verilator/tb.cpp"));
    assert(fs::exists(out_path + "/link/connectivity.cfg"));
    assert(fs::exists(out_path + "/link/clocks.cfg"));
    assert(fs::exists(out_path + "/host/kernel_table.cpp"));
    assert(fs::exists(out_path + "/host/Pass.hpp"));

    std::printf("compiler smoke test: OK\n");
    return 0;
}
