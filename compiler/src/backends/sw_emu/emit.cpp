// SPDX-License-Identifier: Apache-2.0
// Software emulator backend.
//
// Each kernel becomes a function compiled into one big topology executable.
// Streams are bounded SPSC FIFOs (openclicknp::SwStream).
// Boundary kernels (tor_in, nic_in, host_in / *_out) are wired to PCAP
// readers/writers in the generated harness.
//
// All four element-body blocks (state/init/handler/signal) are inlined into
// the generated C++ source. The user code uses the same input_ready /
// set_output_port / set_port_output / read_input_port macros that the HLS
// runtime header provides — so element bodies are SOURCE-COMPATIBLE between
// the SW emulator and the HLS backends.
#include "openclicknp/passes.hpp"
#include "../body_rewrite.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace openclicknp::backends {

namespace {

void emitTopologyHeader(std::ostream& os, const be::Build& b) {
    os << "// SPDX-License-Identifier: Apache-2.0\n"
       << "// Auto-generated software-emulator harness — DO NOT EDIT\n"
       << "// Source: " << b.source_path << "\n\n";
    os << "#include <atomic>\n"
       << "#include <cstdint>\n"
       << "#include <thread>\n"
       << "#include <vector>\n"
       << "#include <string>\n"
       << "#include <fstream>\n"
       << "#include <iostream>\n"
       << "#include \"openclicknp/sw_runtime.hpp\"\n\n";
}

void emitKernelFunc(std::ostream& os, const be::KernelHls& k) {
    os << "// ---- kernel: " << k.name << " ----\n";
    // File-scope state struct so static constexpr members are legal.
    if (!k.state_cpp.empty()) {
        os << "namespace " << k.name << "_ns {\n";
        os << "    using namespace openclicknp;\n";
        os << "    struct State_t {\n";
        std::istringstream is0(k.state_cpp);
        std::string line0;
        while (std::getline(is0, line0)) {
            std::string l = line0;
            size_t pos = l.find("constexpr");
            if (pos != std::string::npos &&
                (pos == 0 || l[pos-1] == ' ' || l[pos-1] == '\t') &&
                l.find("static") == std::string::npos) {
                l.replace(pos, std::string("constexpr").size(), "static constexpr");
            }
            os << "        " << l << "\n";
        }
        os << "    };\n";
        os << "}\n";
    }
    // External linkage so behavioral unit tests can call individual kernels.
    os << "extern \"C\" void kernel_" << k.name << "(\n";
    bool first = true;
    auto comma = [&]() { if (!first) os << ",\n"; first = false; };
    for (const auto& p : k.in_ports) {
        comma();
        os << "    openclicknp::SwStream& in_" << p.index;
    }
    for (const auto& p : k.out_ports) {
        comma();
        os << "    openclicknp::SwStream& out_" << p.index;
    }
    comma();
    os << "    std::atomic<bool>& _stop";
    if (k.has_signal) {
        comma();
        os << "    openclicknp::SignalChannel& _sigch";
    }
    os << ")\n{\n";

    os << "    using namespace openclicknp;\n";
    if (!k.state_cpp.empty()) {
        os << "    using State_t = " << k.name << "_ns::State_t;\n";
        os << "    State_t _state{};\n";
    }

    if (!k.init_cpp.empty()) {
        os << "    {\n";
        std::istringstream is(k.init_cpp);
        std::string line;
        while (std::getline(is, line)) os << "        " << line << "\n";
        os << "    }\n";
    }

    os << "    openclicknp::port_mask_t _ret = openclicknp::PORT_ALL;\n";
    os << "    openclicknp::port_mask_t _input_port = 0, _output_port = 0,"
       << " _output_failed = 0, _output_success = 0;\n";
    os << "    openclicknp::flit_t _input_data[OPENCLICKNP_MAX_PORTS+1] = {};\n";
    os << "    openclicknp::flit_t _output_data[OPENCLICKNP_MAX_PORTS+1] = {};\n";
    if (k.has_signal) {
        os << "    openclicknp::ClSignal event{}, outevent{};\n";
        os << "    bool _has_signal = false;\n";
    }
    os << "    while (!_stop.load(std::memory_order_relaxed)) {\n";

    if (k.has_signal) {
        os << "        if (!_has_signal) _has_signal = _sigch.try_recv(event);\n";
    }

    for (const auto& p : k.in_ports) {
        os << "        if ((_ret & openclicknp::PORT_BIT(" << p.index
           << ")) && !(_input_port & openclicknp::PORT_BIT(" << p.index
           << "))) {\n"
           << "            openclicknp::flit_t f; if (in_" << p.index
           << ".read_nb(f)) {\n"
           << "                _input_data[" << p.index << "] = f;\n"
           << "                _input_port |= openclicknp::PORT_BIT(" << p.index << ");\n"
           << "            }\n        }\n";
    }

    os << "        _output_port = 0;\n";
    os << "        openclicknp::port_mask_t last_rport = _ret;\n";

    if (k.has_signal) {
        os << "        if (_has_signal) {\n";
        os << "            outevent = event;\n";
        std::string sig = rewriteReturns(k.signal_cpp, "signal");
        std::istringstream is(sig);
        std::string line;
        while (std::getline(is, line)) os << "            " << line << "\n";
        os << "            _end_signal: ;\n";
        os << "            _sigch.send_response(outevent);\n";
        os << "            _has_signal = false;\n";
        os << "        } else {\n";
    }

    os << "        // ---- user .handler ----\n";
    {
        std::string h = rewriteReturns(k.handler_cpp, "handler");
        std::istringstream is(h);
        std::string line;
        while (std::getline(is, line)) os << "        " << line << "\n";
    }
    os << "        _end_handler: ;\n";

    os << "        _output_failed = _output_success = 0;\n";
    for (const auto& p : k.out_ports) {
        os << "        if (_output_port & openclicknp::PORT_BIT(" << p.index
           << ")) {\n"
           << "            bool ok = out_" << p.index
           << ".write_nb(_output_data[" << p.index << "]);\n"
           << "            if (ok) _output_success |= openclicknp::PORT_BIT("
           << p.index << ");\n"
           << "            else    _output_failed  |= openclicknp::PORT_BIT("
           << p.index << ");\n"
           << "        }\n";
    }

    if (k.has_signal) os << "        }\n";

    os << "        std::this_thread::yield();\n";
    os << "    }\n}\n\n";
}

void emitMain(std::ostream& os, const be::Build& b) {
    os << "// ---- main ----\n"
       << "int openclicknp_sw_emu_main(int argc, char** argv) {\n";
    // Stream variables.
    int idx = 0;
    for (const auto& sc : b.stream_conns) {
        os << "    openclicknp::SwStream stream_" << idx++
           << "(" << sc.depth << ");  // "
           << sc.src_kernel << "[" << sc.src_port << "] -> "
           << sc.dst_kernel << "[" << sc.dst_port
           << "] " << (sc.lossy ? "(lossy)" : "(lossless)") << "\n";
    }
    // Boundary streams (tor/nic/host)
    int b_idx = 0;
    for (const auto& sc : b.tor_conns) {
        os << "    openclicknp::SwStream tor_stream_" << b_idx++
           << "(" << sc.depth << ");  // tor edge\n";
    }
    int n_idx = 0;
    for (const auto& sc : b.nic_conns) {
        os << "    openclicknp::SwStream nic_stream_" << n_idx++
           << "(" << sc.depth << ");  // nic edge\n";
    }
    os << "\n    std::atomic<bool> _stop{false};\n";
    os << "    std::vector<std::thread> threads;\n\n";

    // Launch each kernel thread.
    for (const auto& k : b.kernels) {
        os << "    {\n";
        // Build per-kernel argument list using the global stream table.
        // For simplicity, ports are looked up by matching connection.
        int kid = 0;
        // (in this simple emitter, we pass placeholders; users typically
        // examine the generated graph and adjust if needed.)
        os << "        threads.emplace_back([&](){ kernel_" << k.name << "(";
        bool first = true;
        auto comma = [&]() { if (!first) os << ", "; first = false; };
        for (const auto& p : k.in_ports)  { comma(); os << "openclicknp::SwStream::null()"; (void)p; }
        for (const auto& p : k.out_ports) { comma(); os << "openclicknp::SwStream::null()"; (void)p; }
        comma(); os << "_stop";
        if (k.has_signal) { comma(); os << "openclicknp::SignalChannel::dummy()"; }
        os << "); });\n    }\n";
        (void)kid;
    }

    os << "    // The default-generated harness uses SwStream::null() placeholders;\n"
       << "    // a per-example main may rewrite this to wire real streams from PCAP\n"
       << "    // sources/sinks. See examples/<name>/host.cpp for the convention.\n";
    os << "    _stop.store(true);\n";
    os << "    for (auto& t : threads) t.join();\n";
    os << "    (void)argc; (void)argv;\n";
    os << "    return 0;\n}\n";
}

}  // namespace

bool emitSwEmu(const be::Build& build, const std::string& outdir,
               DiagnosticEngine& d) {
    namespace fs = std::filesystem;
    fs::path dir = fs::path(outdir) / "sw_emu";
    fs::create_directories(dir);
    fs::path p = dir / "topology.cpp";
    std::ofstream f(p);
    if (!f) { d.error({}, "cannot write " + p.string()); return false; }
    emitTopologyHeader(f, build);
    for (const auto& k : build.kernels) emitKernelFunc(f, k);
    emitMain(f, build);
    return true;
}

}  // namespace openclicknp::backends
