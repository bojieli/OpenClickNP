// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/diagnostic.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>
#include <utility>

namespace openclicknp {

namespace {
const char* sevName(Severity s) {
    switch (s) {
        case Severity::Note:    return "note";
        case Severity::Warning: return "warning";
        case Severity::Error:   return "error";
    }
    return "?";
}
const char* sevColor(Severity s) {
    switch (s) {
        case Severity::Note:    return "\033[1;36m";
        case Severity::Warning: return "\033[1;33m";
        case Severity::Error:   return "\033[1;31m";
    }
    return "";
}
constexpr const char* kReset = "\033[0m";
constexpr const char* kBold  = "\033[1m";
}  // namespace

void DiagnosticEngine::emit(Diagnostic diag) {
    if (diag.severity == Severity::Error)   ++error_count_;
    if (diag.severity == Severity::Warning) ++warning_count_;
    diags_.push_back(std::move(diag));
}

void DiagnosticEngine::warn(SourceRange r, std::string msg) {
    emit(Diagnostic{Severity::Warning, std::move(msg), r, {}});
}
void DiagnosticEngine::error(SourceRange r, std::string msg) {
    emit(Diagnostic{Severity::Error, std::move(msg), r, {}});
}
void DiagnosticEngine::note(SourceRange r, std::string msg) {
    emit(Diagnostic{Severity::Note, std::move(msg), r, {}});
}

void DiagnosticEngine::renderOne(std::ostream& os, const Diagnostic& d) const {
    const auto& buf = sm_.buffer(d.range.begin.file_id);
    os << kBold << buf.path() << ":" << d.range.begin.line << ":"
       << d.range.begin.column << ": " << sevColor(d.severity)
       << sevName(d.severity) << ":" << kReset << " " << d.message << "\n";

    auto line = buf.lineText(d.range.begin.line);
    if (!line.empty()) {
        os << "  " << line << "\n";
        os << "  ";
        for (uint32_t c = 1; c < d.range.begin.column; ++c) os << ' ';
        uint32_t span = 1;
        if (d.range.end.line == d.range.begin.line &&
            d.range.end.column > d.range.begin.column) {
            span = d.range.end.column - d.range.begin.column;
        }
        os << sevColor(d.severity) << '^';
        for (uint32_t c = 1; c < span; ++c) os << '~';
        os << kReset << "\n";
    }
    for (const auto& n : d.notes) {
        renderOne(os, n);
    }
}

void DiagnosticEngine::render(std::ostream& os) const {
    for (const auto& d : diags_) renderOne(os, d);
    if (error_count_ > 0 || warning_count_ > 0) {
        os << error_count_ << " error(s), "
           << warning_count_ << " warning(s)\n";
    }
}

}  // namespace openclicknp
