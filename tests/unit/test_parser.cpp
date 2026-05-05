// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/parser.hpp"
#include "openclicknp/source.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>

using namespace openclicknp;

static const char kPassClnp[] = R"(
.element Pass <1, 1> {
    .state {
        uint64_t flit_count;
    }
    .init {
        _state.flit_count = 0;
    }
    .handler {
        if (test_input_port(PORT_1)) {
            openclicknp::flit_t f = read_input_port(PORT_1);
            set_output_port(1, f);
        }
        return PORT_1;
    }
    .signal (uint cmd, uint param) {
        outevent.lparam[0] = _state.flit_count;
    }
}

Pass :: tor_rx @
Pass :: nic_rx @

tor_in -> tor_rx -> nic_out
nic_in -> nic_rx -> tor_out
)";

int main() {
    // Write to a temp file so the parser uses a real path.
    const char* p = "/tmp/openclicknp_parser_test.clnp";
    {
        std::ofstream f(p);
        f << kPassClnp;
    }

    SourceManager sm;
    DiagnosticEngine d(sm);
    Parser parser(sm, d);
    auto mod = parser.parseFile(p);
    assert(mod);
    assert(!d.hasError());

    // Expect: 1 ElementDecl, 2 InstanceDecls, 4 ConnectionDecls.
    int elements = 0, instances = 0, conns = 0;
    for (const auto& s : mod->stmts) {
        std::visit([&](const auto& v){
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, ast::ElementDecl>)        ++elements;
            else if constexpr (std::is_same_v<T, ast::InstanceDecl>)  ++instances;
            else if constexpr (std::is_same_v<T, ast::ConnectionDecl>) ++conns;
        }, s);
    }
    assert(elements == 1);
    assert(instances == 2);
    assert(conns == 4);

    std::printf("parser tests: OK (%d elem, %d inst, %d conn)\n",
                elements, instances, conns);
    return 0;
}
