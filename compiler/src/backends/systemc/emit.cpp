// SPDX-License-Identifier: Apache-2.0
// SystemC backend.
//
// Each kernel becomes an SC_MODULE with sc_fifo<flit_t> ports. The handler
// loop runs in an SC_THREAD that waits one simulated cycle (wait()) per
// iteration. This gives cycle-accurate behavior for FIFO depth tuning and
// latency measurement without HLS synthesis.

#include "openclicknp/passes.hpp"
#include "../body_rewrite.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace openclicknp::backends {

namespace {

void emitHeader(std::ostream& os, const be::Build& b) {
    os << "// SPDX-License-Identifier: Apache-2.0\n"
       << "// Auto-generated SystemC harness — DO NOT EDIT\n"
       << "// Source: " << b.source_path << "\n\n"
       << "#include <systemc.h>\n"
       << "#include \"openclicknp/sc_runtime.hpp\"\n\n";
}

void emitKernelModule(std::ostream& os, const be::KernelHls& k) {
    os << "SC_MODULE(SC_" << k.name << ") {\n";
    for (const auto& p : k.in_ports)
        os << "    sc_fifo_in<openclicknp::flit_t>  in_"  << p.index << ";\n";
    for (const auto& p : k.out_ports)
        os << "    sc_fifo_out<openclicknp::flit_t> out_" << p.index << ";\n";

    if (!k.state_cpp.empty()) {
        // SC_MODULE struct is class-scoped, so static constexpr is legal.
        os << "    struct State_t {\n";
        std::istringstream is(k.state_cpp);
        std::string line;
        while (std::getline(is, line)) {
            std::string l = line;
            size_t pos = l.find("constexpr");
            if (pos != std::string::npos &&
                (pos == 0 || l[pos-1] == ' ' || l[pos-1] == '\t') &&
                l.find("static") == std::string::npos) {
                l.replace(pos, std::string("constexpr").size(), "static constexpr");
            }
            os << "        " << l << "\n";
        }
        os << "    } _state;\n";
    }

    os << "    void run() {\n";
    if (!k.init_cpp.empty()) {
        os << "        {\n";
        std::istringstream is(k.init_cpp);
        std::string line;
        while (std::getline(is, line)) os << "            " << line << "\n";
        os << "        }\n";
    }
    os << "        openclicknp::port_mask_t _ret = openclicknp::PORT_ALL;\n";
    os << "        openclicknp::port_mask_t _input_port = 0, _output_port = 0,"
       << " _output_failed = 0, _output_success = 0;\n";
    {
        size_t n_in_max  = std::max<size_t>(1, k.in_ports.size());
        size_t n_out_max = std::max<size_t>(1, k.out_ports.size());
        os << "        openclicknp::flit_t _input_data[" << (n_in_max + 1) << "] = {};\n";
        os << "        openclicknp::flit_t _output_data[" << (n_out_max + 1) << "] = {};\n";
    }
    os << "        while (true) {\n";
    os << "            wait(1, SC_NS);  // 1 cycle = 1 ns @ 1 GHz scaled timebase\n";
    for (const auto& p : k.in_ports) {
        os << "            if ((_ret & openclicknp::PORT_BIT(" << p.index
           << ")) && in_" << p.index << ".num_available() > 0 "
           << "&& !(_input_port & openclicknp::PORT_BIT(" << p.index << "))) {\n"
           << "                _input_data[" << p.index << "] = in_" << p.index << ".read();\n"
           << "                _input_port |= openclicknp::PORT_BIT(" << p.index << ");\n"
           << "            }\n";
    }
    os << "            _output_port = 0;\n";
    os << "            openclicknp::port_mask_t last_rport = _ret;\n";
    {
        std::string h = rewriteReturns(k.handler_cpp, "handler");
        std::istringstream is(h);
        std::string line;
        while (std::getline(is, line)) os << "            " << line << "\n";
    }
    os << "            _end_handler: ;\n";
    for (const auto& p : k.out_ports) {
        os << "            if (_output_port & openclicknp::PORT_BIT(" << p.index << ")) {\n"
           << "                out_" << p.index << ".write(_output_data[" << p.index << "]);\n"
           << "            }\n";
    }
    os << "        }\n    }\n";
    os << "    SC_CTOR(SC_" << k.name << ") { SC_THREAD(run); }\n";
    os << "};\n\n";
}

void emitMain(std::ostream& os, const be::Build& b) {
    os << "int sc_main(int argc, char** argv) {\n";
    os << "    (void)argc; (void)argv;\n";
    os << "    // FIFO instantiations\n";
    int idx = 0;
    for (const auto& sc : b.stream_conns) {
        os << "    sc_fifo<openclicknp::flit_t> fifo_" << idx
           << "(" << sc.depth << ");\n";
        ++idx;
    }
    os << "    // Kernel instantiations + binding (placeholder)\n";
    for (const auto& k : b.kernels) {
        os << "    SC_" << k.name << " m_" << k.name
           << "(\"m_" << k.name << "\");\n";
    }
    os << "    sc_start(1, SC_MS);  // run for 1 ms simulated time\n";
    os << "    return 0;\n}\n";
}

}  // namespace

bool emitSystemC(const be::Build& build, const std::string& outdir,
                 DiagnosticEngine& d) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(outdir) / "systemc";
    fs::create_directories(dir);
    fs::path p = dir / "topology.cpp";
    std::ofstream f(p);
    if (!f) { d.error({}, "cannot write " + p.string()); return false; }
    emitHeader(f, build);
    for (const auto& k : build.kernels) emitKernelModule(f, k);
    emitMain(f, build);
    return true;
}

}  // namespace openclicknp::backends
