// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/parser.hpp"
#include "openclicknp/passes.hpp"
#include "openclicknp/source.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>

using namespace openclicknp;

static int countErrors(DiagnosticEngine& d) {
    return static_cast<int>(d.errorCount());
}

static eg::Graph parseAndResolve(const std::string& src,
                                 SourceManager& sm,
                                 DiagnosticEngine& d) {
    static int counter = 0;
    std::string path = "/tmp/openclicknp_analyses_" +
                       std::to_string(counter++) + ".clnp";
    { std::ofstream f(path); f << src; }
    Parser parser(sm, d);
    auto mod = parser.parseFile(path);
    eg::Graph g;
    if (mod) resolveModuleToGraph(*mod, d, g);
    return g;
}

int main() {
    // Test 1: bad port number (port 5 on a 1-output kernel).
    {
        SourceManager sm;
        DiagnosticEngine d(sm);
        auto g = parseAndResolve(R"(
.element Pass <1,1> { .state{} .init{} .handler{} }
Pass :: a
Pass :: b
a[5] -> b
)", sm, d);
        // resolve is OK; the analysis catches it.
        analyzePortArity(g, d);
        assert(countErrors(d) >= 1);
    }
    // Test 2: a clean graph passes all analyses.
    {
        SourceManager sm;
        DiagnosticEngine d(sm);
        auto g = parseAndResolve(R"(
.element Pass <1,1> { .state{} .init{} .handler{} }
Pass :: a
Pass :: b
tor_in -> a -> b -> tor_out
)", sm, d);
        assert(analyzePortArity(g, d));
        assert(analyzeAutorun  (g, d));
        assert(analyzeCycles   (g, d));
        assert(analyzeBandwidth(g, d, 322265625));
        assert(countErrors(d) == 0);
    }
    // Test 3: bandwidth lowering — clock 100 MHz → warn (below 100 G).
    {
        SourceManager sm;
        DiagnosticEngine d(sm);
        auto g = parseAndResolve(R"(
.element Pass <1,1> { .state{} .init{} .handler{} }
Pass :: a
tor_in -> a -> tor_out
)", sm, d);
        analyzeBandwidth(g, d, 100'000'000);  // 100 MHz × 512 bits = 51.2 Gb/s
        // we expect a warning (not error); check warnings present.
        assert(d.warningCount() >= 1);
    }
    std::printf("analyses tests: OK\n");
    return 0;
}
