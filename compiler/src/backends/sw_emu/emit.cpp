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
       << "#include \"openclicknp/sw_runtime.hpp\"\n"
       << "#include \"openclicknp/bigint.hpp\"\n"
       << "#include \"openclicknp/aes128.hpp\"\n"
       << "#include \"openclicknp/sha1.hpp\"\n\n";
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
    {
        size_t n_in_max  = std::max<size_t>(1, k.in_ports.size());
        size_t n_out_max = std::max<size_t>(1, k.out_ports.size());
        os << "    openclicknp::flit_t _input_data[" << (n_in_max + 1) << "] = {};\n";
        os << "    openclicknp::flit_t _output_data[" << (n_out_max + 1) << "] = {};\n";
    }
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
    // Build port -> stream-variable lookup tables so each kernel's
    // in/out ports get wired to the SwStream that connects them.
    // Map key: (kernel_name, "in"|"out", port_index) -> stream var name.
    std::map<std::tuple<std::string,std::string,int>, std::string> port2stream;

    // 1) Internal kernel-to-kernel streams.
    int s_idx = 0;
    std::vector<std::pair<std::string,int>> stream_decls;  // (var, depth)
    for (const auto& sc : b.stream_conns) {
        std::string var = "stream_" + std::to_string(s_idx++);
        stream_decls.push_back({var, sc.depth});
        port2stream[{sc.src_kernel, "out", sc.src_port}] = var;
        port2stream[{sc.dst_kernel, "in",  sc.dst_port}] = var;
    }
    // 2) Tor edges (one of src/dst kernel is the literal "tor_in" or
    //    "tor_out" pseudo-element; the other side is a real kernel).
    int t_idx = 0;
    for (const auto& sc : b.tor_conns) {
        std::string var = "tor_stream_" + std::to_string(t_idx++);
        stream_decls.push_back({var, sc.depth});
        if (sc.src_kernel != "tor_in" && sc.src_kernel != "tor_out")
            port2stream[{sc.src_kernel, "out", sc.src_port}] = var;
        if (sc.dst_kernel != "tor_in" && sc.dst_kernel != "tor_out")
            port2stream[{sc.dst_kernel, "in",  sc.dst_port}] = var;
    }
    // 3) Nic edges.
    int n_idx = 0;
    for (const auto& sc : b.nic_conns) {
        std::string var = "nic_stream_" + std::to_string(n_idx++);
        stream_decls.push_back({var, sc.depth});
        if (sc.src_kernel != "nic_in" && sc.src_kernel != "nic_out")
            port2stream[{sc.src_kernel, "out", sc.src_port}] = var;
        if (sc.dst_kernel != "nic_in" && sc.dst_kernel != "nic_out")
            port2stream[{sc.dst_kernel, "in",  sc.dst_port}] = var;
    }
    // 4) Host streams (host_in / host_out). HostConn carries direction.
    int h_idx = 0;
    for (const auto& hc : b.host_streams) {
        std::string var = "host_stream_" + std::to_string(h_idx++);
        stream_decls.push_back({var, 64});
        // kernel_to_host=true: kernel produces, host consumes -> kernel out port
        // kernel_to_host=false: host produces, kernel consumes -> kernel in port
        port2stream[{hc.kernel, hc.kernel_to_host ? "out" : "in", hc.port}] = var;
    }

    os << "// ---- main ----\n";

    // Boundary stream accessors so a per-test harness can reach in
    // and inject/observe flits without re-running the whole emitter.
    os << "// Boundary stream accessors (defined out-of-line below) — a\n";
    os << "// test harness can reach in via openclicknp_sw_emu_<name>_stream().\n";
    os << "namespace openclicknp_sw_emu {\n";
    for (const auto& [var, depth] : stream_decls) {
        os << "    extern openclicknp::SwStream& " << var << "();\n";
    }
    os << "    extern std::atomic<bool>& stop_flag();\n";
    os << "}\n\n";

    // Singleton storage for the streams (function-local statics so any
    // translation unit that includes the generated code links cleanly).
    os << "namespace openclicknp_sw_emu {\n";
    for (const auto& [var, depth] : stream_decls) {
        os << "    openclicknp::SwStream& " << var << "() {\n"
           << "        static openclicknp::SwStream s(" << depth << ");\n"
           << "        return s;\n"
           << "    }\n";
    }
    os << "    std::atomic<bool>& stop_flag() {\n"
       << "        static std::atomic<bool> s{false};\n"
       << "        return s;\n"
       << "    }\n";
    os << "}\n\n";

    os << "int openclicknp_sw_emu_main(int argc, char** argv) {\n";
    os << "    (void)argc; (void)argv;\n";
    os << "    auto& _stop = openclicknp_sw_emu::stop_flag();\n";
    os << "    std::vector<std::thread> threads;\n\n";

    auto stream_for = [&](const std::string& kernel, const std::string& dir,
                          int port_idx) -> std::string {
        auto it = port2stream.find({kernel, dir, port_idx});
        if (it == port2stream.end())
            return "openclicknp::SwStream::null()";
        return "openclicknp_sw_emu::" + it->second + "()";
    };

    // Launch each kernel thread with the right streams.
    for (const auto& k : b.kernels) {
        os << "    {\n";
        os << "        threads.emplace_back([&](){ kernel_" << k.name << "(";
        bool first = true;
        auto comma = [&]() { if (!first) os << ", "; first = false; };
        for (const auto& p : k.in_ports)  { comma(); os << stream_for(k.name, "in",  p.index); }
        for (const auto& p : k.out_ports) { comma(); os << stream_for(k.name, "out", p.index); }
        comma(); os << "_stop";
        if (k.has_signal) { comma(); os << "openclicknp::SignalChannel::dummy()"; }
        os << "); });\n    }\n";
    }

    // Default behavior (when run as a standalone binary with no test
    // harness driving boundaries): exit immediately. A test harness
    // replaces this by importing the kernel functions, wiring its own
    // input streams via the openclicknp_sw_emu::host_stream_*() accessors,
    // and orchestrating the run itself.
    os << "    _stop.store(true);\n";
    os << "    for (auto& t : threads) t.join();\n";
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
