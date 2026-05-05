// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openclicknp/source.hpp"

#include <string>
#include <vector>

namespace openclicknp {

enum class Severity { Note, Warning, Error };

struct Diagnostic {
    Severity   severity;
    std::string message;
    SourceRange range;
    std::vector<Diagnostic> notes;
};

class DiagnosticEngine {
public:
    explicit DiagnosticEngine(const SourceManager& sm) : sm_(sm) {}

    void emit(Diagnostic diag);
    void warn(SourceRange r, std::string msg);
    void error(SourceRange r, std::string msg);
    void note(SourceRange r, std::string msg);

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept {
        return diags_;
    }
    [[nodiscard]] bool hasError() const noexcept { return error_count_ > 0; }
    [[nodiscard]] uint32_t errorCount()   const noexcept { return error_count_; }
    [[nodiscard]] uint32_t warningCount() const noexcept { return warning_count_; }

    // Pretty-print every diagnostic to a stream.
    void render(std::ostream& os) const;

private:
    const SourceManager& sm_;
    std::vector<Diagnostic> diags_;
    uint32_t error_count_   = 0;
    uint32_t warning_count_ = 0;

    void renderOne(std::ostream& os, const Diagnostic& d) const;
};

}  // namespace openclicknp
