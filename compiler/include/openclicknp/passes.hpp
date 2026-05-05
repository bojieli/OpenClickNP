// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openclicknp/be_ir.hpp"
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/eg_ir.hpp"

namespace openclicknp {

// AST → EG IR
bool resolveModuleToGraph(const ast::Module& mod,
                          DiagnosticEngine& diags,
                          eg::Graph& out_graph);

// EG IR analyses (return false on hard errors).
bool analyzePortArity (const eg::Graph& g, DiagnosticEngine& d);
bool analyzeCycles    (const eg::Graph& g, DiagnosticEngine& d);
bool analyzeBandwidth (const eg::Graph& g, DiagnosticEngine& d, int user_clock_hz);
bool analyzeAutorun   (eg::Graph&       g, DiagnosticEngine& d);

// EG IR → BE IR
bool lowerToBackend(const eg::Graph& g,
                    DiagnosticEngine& d,
                    be::Platform platform,
                    int user_clock_hz,
                    const std::string& source_path,
                    be::Build& out);

// Backend emitters
namespace backends {

bool emitHlsCpp     (const be::Build&, const std::string& outdir, DiagnosticEngine&);
bool emitSystemC    (const be::Build&, const std::string& outdir, DiagnosticEngine&);
bool emitSwEmu      (const be::Build&, const std::string& outdir, DiagnosticEngine&);
bool emitVerilator  (const be::Build&, const std::string& outdir, DiagnosticEngine&);
bool emitVppLink    (const be::Build&, const std::string& outdir, DiagnosticEngine&);
bool emitXrtHost    (const be::Build&, const std::string& outdir, DiagnosticEngine&);

}  // namespace backends

}  // namespace openclicknp
