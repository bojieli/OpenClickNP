// SPDX-License-Identifier: Apache-2.0
// End-to-end: compile examples/PassTraffic/topology.clnp and check the
// output.
#include "openclicknp/driver.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>

using namespace openclicknp;

int main() {
    namespace fs = std::filesystem;
    fs::path repo_root = fs::path(__FILE__).parent_path().parent_path().parent_path();
    fs::path src       = repo_root / "examples" / "PassTraffic" / "topology.clnp";
    fs::path elements  = repo_root / "elements";
    if (!fs::exists(src)) {
        std::cerr << "skipping: " << src << " not found (CI may run outside repo)\n";
        return 0;
    }

    fs::path out = "/tmp/openclicknp_e2e_passtraffic";
    fs::remove_all(out);

    DriverOptions opts;
    opts.input_path = src.string();
    opts.output_dir = out.string();
    opts.import_dirs.push_back(elements.string());

    Driver d;
    int rc = d.run(opts);
    assert(rc == 0);
    assert(fs::exists(out / "kernels"));
    assert(fs::exists(out / "sw_emu" / "topology.cpp"));
    assert(fs::exists(out / "host" / "kernel_table.cpp"));
    std::printf("e2e_passtraffic: OK\n");
    return 0;
}
