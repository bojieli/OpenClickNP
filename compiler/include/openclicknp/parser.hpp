// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openclicknp/ast.hpp"
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/lexer.hpp"
#include "openclicknp/source.hpp"

#include <memory>
#include <optional>
#include <unordered_set>

namespace openclicknp {

class Parser {
public:
    Parser(SourceManager& sm, DiagnosticEngine& diags);

    // Parse a single .clnp file. Recursively pulls in imports and merges
    // them into the same Module (so the resolver sees a flat list of
    // top-level statements).
    [[nodiscard]] std::optional<ast::Module> parseFile(const std::string& path);

private:
    SourceManager&    sm_;
    DiagnosticEngine& diags_;

    // Used to break import cycles.
    std::unordered_set<std::string> seen_files_;

    // Active token-stream state for a single buffer.
    struct State {
        std::unique_ptr<Lexer> lex;
        Token cur;
        Token prev;
        uint32_t file_id;
    };

    void advance(State& s);
    bool check(const State& s, Tok t) const { return s.cur.kind == t; }
    bool accept(State& s, Tok t);
    Token expect(State& s, Tok t, const char* what);

    // Productions
    bool parseTopStmt(State& s, ast::Module& mod);
    bool parseImport(State& s, ast::Module& mod);
    bool parseElementDecl(State& s, ast::Module& mod);
    bool parseElementGroupDecl(State& s, ast::Module& mod);
    bool parseInstanceOrConnection(State& s, ast::Module& mod);

    bool parseElementBody(State& s, ast::ElementDecl& elem);
    bool parseGroupBody(State& s, ast::ElementGroupBody& body);

    std::optional<ast::OpaqueCpp> parseBraceBlock(State& s);
    std::optional<ast::SignalParam> parseSignalParam(State& s);

    // Topology:  HOST_IDENT IDENT [@] [&] [( ... )]
    //         | NODE -> [range] NODE -> ...
    bool parseDeclOrChain(State& s, ast::Module* mod_out,
                          ast::ElementGroupBody* group_out);

    bool parseHostQualifier(State& s, bool& host_ctrl, bool& autorun);
    std::optional<ast::PortRange> parseOptionalRange(State& s);
    std::optional<ast::ConnEnd>   parseConnEnd(State& s);

    // Helpers
    void error(const Token& at, std::string msg);
    void note(const Token& at, std::string msg);
};

}  // namespace openclicknp
