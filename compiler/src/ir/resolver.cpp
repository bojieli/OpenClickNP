// SPDX-License-Identifier: Apache-2.0
// Resolver: AST Module → eg::Graph
//   - Builds element type table
//   - Inlines element groups (renaming `begin`/`end` to outer ports)
//   - Auto-names anonymous instances uniquely
//   - Validates references; produces typed graph
#include "openclicknp/ast.hpp"
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/eg_ir.hpp"

#include <map>
#include <set>
#include <sstream>
#include <variant>

namespace openclicknp {

namespace {

struct GroupInstance {
    int n_in_ports = 0;
    int n_out_ports = 0;
    std::vector<std::string> internal_names;   // for renaming on inline
};

class Resolver {
public:
    Resolver(const ast::Module& mod, DiagnosticEngine& diags)
        : mod_(mod), diags_(diags) {}

    bool resolve(eg::Graph& out_graph) {
        // Step 1: collect element decls and group decls.
        for (const auto& s : mod_.stmts) {
            std::visit([&](const auto& v){ collectDecl(v); }, s);
        }
        // Step 2: process top-level instances and connections, expanding groups.
        for (const auto& s : mod_.stmts) {
            std::visit([&](const auto& v){ processTop(v, out_graph); }, s);
        }

        // Step 3: synthesize boundary pseudo-element kernels for any
        // referenced specials (tor_in/out, nic_in/out, host_in/out, Drop, Idle).
        synthesizeSpecials(out_graph);

        // Step 4: assign GIDs and AXI-Lite bases to @-marked kernels.
        assignSignalSlots(out_graph);

        return !diags_.hasError();
    }

private:
    const ast::Module& mod_;
    DiagnosticEngine&  diags_;
    std::map<std::string, const ast::ElementDecl*> elem_decls_;
    std::map<std::string, const ast::ElementGroupDecl*> group_decls_;

    int anon_counter_ = 0;

    void collectDecl(const ast::ElementDecl& d) {
        if (elem_decls_.count(d.name)) {
            diags_.error(d.src, "duplicate `.element " + d.name + "`");
        } else elem_decls_[d.name] = &d;
    }
    void collectDecl(const ast::ElementGroupDecl& d) {
        if (group_decls_.count(d.name)) {
            diags_.error(d.src, "duplicate `.element_group " + d.name + "`");
        } else group_decls_[d.name] = &d;
    }
    void collectDecl(const ast::ImportDecl&) {}
    void collectDecl(const ast::InstanceDecl&) {}
    void collectDecl(const ast::ConnectionDecl&) {}

    void processTop(const ast::ImportDecl&, eg::Graph&) {}

    void processTop(const ast::ElementDecl&, eg::Graph&) {}

    void processTop(const ast::ElementGroupDecl&, eg::Graph&) {
        // Group definitions don't appear directly in the graph; they are
        // expanded when instantiated.
    }

    void processTop(const ast::InstanceDecl& d, eg::Graph& out) {
        // Two cases:
        //  (a) d.type names an .element       → emit a single Kernel
        //  (b) d.type names an .element_group → inline its body, with the
        //      group's begin/end ports rewritten to this instance's ports.
        if (auto it = elem_decls_.find(d.type); it != elem_decls_.end()) {
            emitKernelFromInstance(*it->second, d, out, /*name_prefix=*/"");
            return;
        }
        if (auto it = group_decls_.find(d.type); it != group_decls_.end()) {
            inlineGroup(*it->second, d, out);
            return;
        }
        // Could be a forward-declared anonymous Kernel emitted earlier; skip.
        // Or it's a special kernel (tor_in etc.) — these don't appear as
        // InstanceDecls.
        // Final fallback: error.
        diags_.error(d.src, "unknown element or group type `" + d.type + "`");
    }

    void processTop(const ast::ConnectionDecl& c, eg::Graph& out) {
        addConnection(c, out, /*name_prefix=*/"");
    }

    // Resolve src/dst node names against synthesized kernels. If the name
    // is a special keyword, just record the connection — synthesizeSpecials
    // will create the corresponding pseudo-Kernel.
    void addConnection(const ast::ConnectionDecl& c, eg::Graph& out,
                       const std::string& name_prefix) {
        eg::Edge e;
        e.src_kernel = name_prefix + c.source.node_name;
        e.dst_kernel = name_prefix + c.dest.node_name;
        e.src_port   = c.source.range.lo == 0 ? 1 : c.source.range.lo;
        e.dst_port   = c.dest.range.lo   == 0 ? 1 : c.dest.range.lo;
        e.lossy      = c.lossy;
        if (c.dest.range.depth > 0) e.depth = c.dest.range.depth;
        else if (c.source.range.depth > 0) e.depth = c.source.range.depth;
        else e.depth = 64;
        e.kind = eg::ChannelKind::Flit;   // default; metadata is opt-in via
                                          // a future syntax extension
        e.src  = c.src;
        out.edges.push_back(std::move(e));
    }

    void emitKernelFromInstance(const ast::ElementDecl& decl,
                                const ast::InstanceDecl& inst,
                                eg::Graph& out,
                                const std::string& name_prefix) {
        eg::Kernel k;
        k.name = name_prefix + (inst.anonymous
                                ? makeAnonName(inst.type)
                                : inst.name);
        k.type = inst.type;
        k.host_control = inst.host_control;
        k.autorun      = inst.autorun;
        k.n_in_ports   = inst.n_in_ports  > 0 ? inst.n_in_ports  : decl.n_in_ports;
        k.n_out_ports  = inst.n_out_ports > 0 ? inst.n_out_ports : decl.n_out_ports;
        if (k.n_in_ports < 0)  k.n_in_ports = 1;
        if (k.n_out_ports < 0) k.n_out_ports = 1;
        k.channel_depth = inst.channel_depth;
        k.params       = inst.params;
        k.signal_params = decl.signal_params;
        if (decl.state)   k.state_cpp   = decl.state->text;
        if (decl.init)    k.init_cpp    = decl.init->text;
        if (decl.handler) k.handler_cpp = decl.handler->text;
        if (decl.signal)  k.signal_cpp  = decl.signal->text;
        k.src = inst.src;
        out.kernels.push_back(std::move(k));
    }

    void inlineGroup(const ast::ElementGroupDecl& g,
                     const ast::InstanceDecl& inst,
                     eg::Graph& out) {
        // For each statement in the group body, rewrite "begin"/"end" node
        // names to the outer instance's ports.  The inline mapping uses a
        // unique prefix derived from the instance name to avoid collisions
        // when the group is instantiated multiple times.
        std::string pfx = inst.name + "::";
        for (const auto& gs : g.body.stmts) {
            std::visit([&](const auto& v){
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, ast::InstanceDecl>) {
                    ast::InstanceDecl copy = v;
                    copy.name = pfx + (copy.anonymous
                                        ? makeAnonName(copy.type)
                                        : copy.name);
                    copy.anonymous = false;
                    processTop(copy, out);
                } else if constexpr (std::is_same_v<T, ast::ConnectionDecl>) {
                    ast::ConnectionDecl copy = v;
                    auto rewriteEnd = [&](ast::ConnEnd& e) {
                        // begin/end are rewritten to the outer instance name
                        // with the corresponding port number passed through.
                        if (e.node_name == "begin" || e.node_name == "end") {
                            e.node_name = inst.name;
                        } else {
                            e.node_name = pfx + e.node_name;
                        }
                    };
                    rewriteEnd(copy.source);
                    rewriteEnd(copy.dest);
                    addConnection(copy, out, /*name_prefix=*/"");
                }
            }, gs.stmt);
        }
    }

    void synthesizeSpecials(eg::Graph& out) {
        std::set<std::string> referenced;
        for (const auto& e : out.edges) {
            referenced.insert(e.src_kernel);
            referenced.insert(e.dst_kernel);
        }
        std::set<std::string> existing;
        for (const auto& k : out.kernels) existing.insert(k.name);

        auto add_special = [&](const std::string& n) {
            if (existing.count(n)) return;
            auto kind = eg::specialFromName(n);
            if (kind == eg::SpecialKind::None) return;
            eg::Kernel k;
            k.name = n;
            k.type = n;
            k.special = kind;
            // Boundary pseudo-elements: 0 in, 1 out for *_in / Idle;
            //                            1 in, 0 out for *_out / Drop.
            switch (kind) {
                case eg::SpecialKind::TorIn:
                case eg::SpecialKind::NicIn:
                case eg::SpecialKind::HostIn:
                case eg::SpecialKind::Idle:
                    k.n_in_ports = 0; k.n_out_ports = 1; break;
                case eg::SpecialKind::TorOut:
                case eg::SpecialKind::NicOut:
                case eg::SpecialKind::HostOut:
                case eg::SpecialKind::Drop:
                    k.n_in_ports = 1; k.n_out_ports = 0; break;
                default:
                    k.n_in_ports = 1; k.n_out_ports = 1; break;
            }
            k.autorun = true;
            out.kernels.push_back(std::move(k));
            existing.insert(n);
        };

        for (const auto& n : referenced) add_special(n);

        // Final pass: any reference still unresolved is an error.
        for (const auto& e : out.edges) {
            if (!existing.count(e.src_kernel)) {
                diags_.error(e.src, "unknown source node `" + e.src_kernel + "`");
            }
            if (!existing.count(e.dst_kernel)) {
                diags_.error(e.src, "unknown destination node `" + e.dst_kernel + "`");
            }
        }
    }

    void assignSignalSlots(eg::Graph& out) {
        int gid = 0;
        int axi_base = 0x1000;
        for (auto& k : out.kernels) {
            if (k.host_control && k.special == eg::SpecialKind::None) {
                k.gid          = gid++;
                k.axilite_base = axi_base;
                axi_base      += 0x80;          // 128 B per kernel block
            }
        }
    }

    std::string makeAnonName(const std::string& type) {
        std::ostringstream os;
        os << type << "__anon_" << anon_counter_++;
        return os.str();
    }
};

}  // namespace

bool resolveModuleToGraph(const ast::Module& mod,
                          DiagnosticEngine& diags,
                          eg::Graph& out_graph) {
    Resolver r(mod, diags);
    return r.resolve(out_graph);
}

}  // namespace openclicknp
