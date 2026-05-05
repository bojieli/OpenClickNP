// SPDX-License-Identifier: Apache-2.0
// Cycle detection on the kernel graph.
//
// Pure-dataflow cycles risk deadlock under backpressure, so we warn on any
// SCC that contains 2+ kernels. SCCs that go through a host-controlled (`@`)
// kernel are tolerated (the signal RPC can break the cycle).
#include "openclicknp/passes.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <stack>
#include <vector>

namespace openclicknp {

namespace {

struct Tarjan {
    const eg::Graph& g;
    std::map<std::string, std::vector<std::string>> succ;
    std::map<std::string, int> idx, low;
    std::map<std::string, bool> onstack;
    std::vector<std::string> stk;
    std::vector<std::vector<std::string>> sccs;
    int counter = 0;

    explicit Tarjan(const eg::Graph& gg) : g(gg) {
        for (const auto& e : g.edges) {
            succ[e.src_kernel].push_back(e.dst_kernel);
        }
    }

    void strong(const std::string& v) {
        idx[v] = low[v] = counter++;
        stk.push_back(v);
        onstack[v] = true;
        for (const auto& w : succ[v]) {
            if (!idx.count(w)) {
                strong(w);
                low[v] = std::min(low[v], low[w]);
            } else if (onstack[w]) {
                low[v] = std::min(low[v], idx[w]);
            }
        }
        if (low[v] == idx[v]) {
            std::vector<std::string> comp;
            while (true) {
                auto w = stk.back(); stk.pop_back();
                onstack[w] = false;
                comp.push_back(w);
                if (w == v) break;
            }
            sccs.push_back(std::move(comp));
        }
    }

    void run() {
        for (const auto& k : g.kernels) {
            if (!idx.count(k.name)) strong(k.name);
        }
    }
};

}  // namespace

bool analyzeCycles(const eg::Graph& g, DiagnosticEngine& d) {
    Tarjan t(g); t.run();
    for (const auto& comp : t.sccs) {
        if (comp.size() < 2) continue;
        // Acceptable if any kernel in the SCC is host-controlled.
        bool tolerated = false;
        for (const auto& n : comp) {
            const auto* k = g.find(n);
            if (k && k->host_control) { tolerated = true; break; }
        }
        if (tolerated) continue;

        std::ostringstream os;
        os << "pure-dataflow cycle (deadlock risk) involving:";
        for (const auto& n : comp) os << " " << n;
        SourceRange r{};
        if (auto* k = g.find(comp.front())) r = k->src;
        d.warn(r, os.str());
    }
    return true;
}

}  // namespace openclicknp
