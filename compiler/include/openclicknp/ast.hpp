// SPDX-License-Identifier: Apache-2.0
// Surface AST produced by the parser.
#pragma once

#include "openclicknp/source.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace openclicknp::ast {

// ----------------------------------------------------------------------------
// Element bodies: state/init/handler/signal each carry an opaque C++ block.
// ----------------------------------------------------------------------------

struct OpaqueCpp {
    std::string text;       // raw C++17 between matching braces (excl. braces)
    SourceRange src;        // location of the opening `{`
};

struct SignalParam {
    std::string type_name;   // e.g. "uchar", "uint", a user typedef
    std::string field_name;  // event.<field_name> / outevent.<field_name>
    SourceRange src;
};

struct ElementDecl {
    std::string name;
    int n_in_ports  = -1;     // -1 = ANY; resolved by instance later
    int n_out_ports = -1;
    std::optional<OpaqueCpp> state;
    std::optional<OpaqueCpp> init;
    std::optional<OpaqueCpp> handler;
    std::optional<OpaqueCpp> signal;
    std::vector<SignalParam> signal_params;   // optional typed param list
    SourceRange src;
};

// ----------------------------------------------------------------------------
// Topology: instance declarations + connection statements.
// ----------------------------------------------------------------------------

struct InstanceParam {
    enum class Kind { Number, String };
    Kind        kind;
    std::string text;       // raw text (number as string, string with quotes
                            // already stripped)
    SourceRange src;
};

struct InstanceDecl {
    std::string type;          // element type
    std::string name;          // instance name (auto-generated if anonymous)
    bool        anonymous = false;
    bool        host_control = false;       // @
    bool        autorun      = false;       // &
    int         n_in_ports   = -1;          // optional explicit override
    int         n_out_ports  = -1;
    int         channel_depth = 64;         // default
    std::vector<InstanceParam> params;
    SourceRange src;
};

struct PortRange {
    int lo = 0;
    int hi = 0;        // [lo .. hi]; 0 means "default port (1)"
    int depth = -1;    // optional override (e.g., [1*128] = port 1 depth 128)
    SourceRange src;
};

struct ConnEnd {
    std::string node_name;     // existing instance name OR pseudo-element
    PortRange   range;
    SourceRange src;
};

struct ConnectionDecl {
    ConnEnd source;
    ConnEnd dest;
    bool    lossy = false;     // -> false, => true
    SourceRange src;
};

// ----------------------------------------------------------------------------
// Element groups: reusable subgraphs.
// ----------------------------------------------------------------------------

struct ElementGroupBody;       // forward
struct GroupStmt {
    std::variant<InstanceDecl, ConnectionDecl> stmt;
};
struct ElementGroupBody {
    std::vector<GroupStmt> stmts;
};

struct ElementGroupDecl {
    std::string name;
    int n_in_ports  = -1;
    int n_out_ports = -1;
    ElementGroupBody body;
    SourceRange src;
};

// ----------------------------------------------------------------------------
// Imports.
// ----------------------------------------------------------------------------

struct ImportDecl {
    std::string path;
    SourceRange src;
};

// ----------------------------------------------------------------------------
// Top-level statements.
// ----------------------------------------------------------------------------

using TopStmt = std::variant<
    ImportDecl,
    ElementDecl,
    ElementGroupDecl,
    InstanceDecl,
    ConnectionDecl
>;

struct Module {
    uint32_t file_id = 0;
    std::vector<TopStmt> stmts;
};

}  // namespace openclicknp::ast
