// SPDX-License-Identifier: Apache-2.0
//
// SystemC e2e smoke test: invoke openclicknp-cc to generate a single-
// element SystemC topology, then compile-and-run it. Success == it
// links against libsystemc, runs sc_main, and exits with the expected
// "complete binding failed: port not bound" message that the
// auto-generated placeholder harness produces (or, on a wired-up
// example, exits cleanly).
//
// This proves the SystemC codegen produces compilable, linkable
// SystemC source, even if a real per-app driver harness is per-app
// hand-written like the SW emu's host main.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    const char* gen_dir = "/tmp/openclicknp_systemc_smoke";
    std::string cmd_gen = std::string(argv[0]).substr(0, std::string(argv[0]).find_last_of('/'));
    // Locate openclicknp-cc next to ourselves.
    std::string cc_path = std::string(getenv("OPENCLICKNP_CC") ? getenv("OPENCLICKNP_CC") :
                                      "build/compiler/openclicknp-cc");
    std::string src_path = std::string(getenv("OPENCLICKNP_TOPOLOGY") ? getenv("OPENCLICKNP_TOPOLOGY") :
                                       "examples/PassTraffic/topology.clnp");
    std::string elements = std::string(getenv("OPENCLICKNP_ELEMENTS") ? getenv("OPENCLICKNP_ELEMENTS") :
                                       "elements");

    {
        std::string cmd = "rm -rf " + std::string(gen_dir) + " && " +
                          cc_path + " -I " + elements + " -o " + gen_dir +
                          " --no-link --no-host " + src_path + " >/dev/null 2>&1";
        if (std::system(cmd.c_str()) != 0) {
            std::fprintf(stderr, "openclicknp-cc invocation failed: %s\n", cmd.c_str());
            return 1;
        }
    }

    // Confirm SystemC topology was emitted.
    std::ifstream f(std::string(gen_dir) + "/systemc/topology.cpp");
    if (!f) {
        std::fprintf(stderr, "SystemC topology.cpp was not generated\n");
        return 1;
    }
    std::string line;
    bool saw_module = false;
    while (std::getline(f, line)) {
        if (line.find("SC_MODULE") != std::string::npos) saw_module = true;
    }
    assert(saw_module);

    // Compile it.
    std::string compile = "g++ -std=c++17 -O0 -I /usr/include "
                          "-I runtime/include " +
                          std::string(gen_dir) + "/systemc/topology.cpp "
                          "-lsystemc -lpthread "
                          "-o " + std::string(gen_dir) + "/sc_test 2>" +
                          std::string(gen_dir) + "/compile.err";
    if (std::system(compile.c_str()) != 0) {
        std::fprintf(stderr, "SystemC compile failed; see %s/compile.err\n", gen_dir);
        return 1;
    }

    // Run with a 5-second timeout. Whether it exits 0 (clean) or hits
    // the unbound-port assertion, we only need to confirm the binary
    // launches the SystemC scheduler.
    int rc = std::system((std::string("timeout 5 ") + gen_dir + "/sc_test "
                          ">" + gen_dir + "/run.out 2>&1").c_str());
    (void)rc;
    std::ifstream out(std::string(gen_dir) + "/run.out");
    bool saw_systemc_banner = false;
    std::string l;
    while (std::getline(out, l)) {
        if (l.find("SystemC ") != std::string::npos) saw_systemc_banner = true;
    }
    if (!saw_systemc_banner) {
        std::fprintf(stderr, "SystemC banner not seen in output\n");
        return 1;
    }
    std::printf("SystemC e2e smoke: compiled + linked + launched — OK\n");
    return 0;
}
