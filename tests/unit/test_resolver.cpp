// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/parser.hpp"
#include "openclicknp/passes.hpp"
#include "openclicknp/source.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>

using namespace openclicknp;

static const char kPassClnp[] = R"(
.element Pass <1, 1> {
    .state { uint64_t c; }
    .init  { _state.c = 0; }
    .handler { return PORT_1; }
}

Pass :: rx_a
Pass :: rx_b
tor_in -> rx_a -> nic_out
nic_in -> rx_b -> tor_out
)";

int main() {
    const char* p = "/tmp/openclicknp_resolver_test.clnp";
    { std::ofstream f(p); f << kPassClnp; }

    SourceManager sm;
    DiagnosticEngine d(sm);
    Parser parser(sm, d);
    auto mod = parser.parseFile(p);
    assert(mod);

    eg::Graph g;
    bool ok = resolveModuleToGraph(*mod, d, g);
    assert(ok);
    assert(!d.hasError());

    // Expect 2 user kernels + 4 special pseudo-elements (tor_in, tor_out,
    // nic_in, nic_out) — total 6 kernels.
    assert(g.kernels.size() == 6);

    int specials = 0, user = 0;
    for (const auto& k : g.kernels) {
        if (k.special != eg::SpecialKind::None) ++specials;
        else                                    ++user;
    }
    assert(specials == 4);
    assert(user == 2);
    assert(g.edges.size() == 4);

    std::printf("resolver tests: OK\n");
    return 0;
}
