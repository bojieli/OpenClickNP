// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/driver.hpp"

#include "openclicknp/parser.hpp"
#include "openclicknp/passes.hpp"

#include <iostream>

namespace openclicknp {

int Driver::run(const DriverOptions& opts) {
    SourceManager sm;
    for (const auto& d : opts.import_dirs) sm.addImportDir(d);
    DiagnosticEngine diags(sm);

    Parser parser(sm, diags);
    auto mod = parser.parseFile(opts.input_path);
    if (!mod) {
        diags.render(std::cerr);
        return std::max<uint32_t>(1u, diags.errorCount());
    }
    if (opts.parse_only) {
        diags.render(std::cerr);
        return diags.errorCount();
    }

    eg::Graph g;
    if (!resolveModuleToGraph(*mod, diags, g)) {
        diags.render(std::cerr);
        return std::max<uint32_t>(1u, diags.errorCount());
    }
    analyzePortArity(g, diags);
    analyzeAutorun  (g, diags);
    analyzeCycles   (g, diags);
    analyzeBandwidth(g, diags, /*user_clock_hz=*/322265625);

    if (diags.hasError()) {
        diags.render(std::cerr);
        return diags.errorCount();
    }

    be::Build build;
    if (!lowerToBackend(g, diags, opts.platform, /*user_clock_hz=*/322265625,
                        opts.input_path, build)) {
        diags.render(std::cerr);
        return std::max<uint32_t>(1u, diags.errorCount());
    }

    bool ok = true;
    if (opts.emit_hls_cpp)
        ok &= backends::emitHlsCpp     (build, opts.output_dir, diags);
    if (opts.emit_systemc)
        ok &= backends::emitSystemC    (build, opts.output_dir, diags);
    if (opts.emit_sw_emu)
        ok &= backends::emitSwEmu      (build, opts.output_dir, diags);
    if (opts.emit_verilator_sim)
        ok &= backends::emitVerilator  (build, opts.output_dir, diags);
    if (opts.emit_vpp_link)
        ok &= backends::emitVppLink    (build, opts.output_dir, diags);
    if (opts.emit_xrt_host)
        ok &= backends::emitXrtHost    (build, opts.output_dir, diags);

    diags.render(std::cerr);
    return ok ? static_cast<int>(diags.errorCount())
              : std::max<int>(1, static_cast<int>(diags.errorCount()));
}

}  // namespace openclicknp
