// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/parser.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace openclicknp {

Parser::Parser(SourceManager& sm, DiagnosticEngine& diags)
    : sm_(sm), diags_(diags) {}

void Parser::advance(State& s) {
    s.prev = s.cur;
    s.cur  = s.lex->next();
}

bool Parser::accept(State& s, Tok t) {
    if (s.cur.kind == t) { advance(s); return true; }
    return false;
}

Token Parser::expect(State& s, Tok t, const char* what) {
    if (s.cur.kind == t) {
        Token tk = s.cur;
        advance(s);
        return tk;
    }
    std::ostringstream os;
    os << "expected " << what << " (got " << tokName(s.cur.kind) << ")";
    error(s.cur, os.str());
    Token bad;
    bad.kind = Tok::Error;
    bad.range = s.cur.range;
    return bad;
}

void Parser::error(const Token& at, std::string msg) {
    diags_.error(at.range, std::move(msg));
}
void Parser::note(const Token& at, std::string msg) {
    diags_.note(at.range, std::move(msg));
}

std::optional<ast::Module> Parser::parseFile(const std::string& path) {
    namespace fs = std::filesystem;
    fs::path canon;
    try {
        canon = fs::weakly_canonical(path);
    } catch (...) {
        canon = path;
    }
    std::string canon_str = canon.string();
    if (seen_files_.count(canon_str)) {
        return ast::Module{};   // already merged in
    }
    seen_files_.insert(canon_str);

    std::ifstream f(path);
    if (!f) {
        SourceLoc dummy{0, 1, 1, 0};
        diags_.error({dummy, dummy}, "cannot open `" + path + "`");
        return std::nullopt;
    }
    std::stringstream ss; ss << f.rdbuf();
    uint32_t fid = sm_.addBuffer(path, ss.str());

    State s;
    s.file_id = fid;
    s.lex = std::make_unique<Lexer>(sm_.buffer(fid), fid, diags_);
    s.cur = s.lex->next();

    ast::Module mod;
    mod.file_id = fid;

    while (s.cur.kind != Tok::Eof) {
        if (!parseTopStmt(s, mod)) {
            // Recover by skipping until next top-level marker.
            while (s.cur.kind != Tok::Eof &&
                   s.cur.kind != Tok::KwElement &&
                   s.cur.kind != Tok::KwElementGroup &&
                   s.cur.kind != Tok::KwImport) {
                advance(s);
            }
        }
    }
    return mod;
}

bool Parser::parseTopStmt(State& s, ast::Module& mod) {
    switch (s.cur.kind) {
        case Tok::KwImport:        return parseImport(s, mod);
        case Tok::KwElement:       return parseElementDecl(s, mod);
        case Tok::KwElementGroup:  return parseElementGroupDecl(s, mod);
        default:                   return parseInstanceOrConnection(s, mod);
    }
}

bool Parser::parseImport(State& s, ast::Module& mod) {
    Token kw = s.cur;
    advance(s); // import
    Token str = expect(s, Tok::StringLit, "import path string");
    if (str.kind != Tok::StringLit) return false;
    accept(s, Tok::Semicolon);  // optional

    // Resolve and recursively parse.
    const auto& referer_buf = sm_.buffer(mod.file_id);
    std::string referer_path = referer_buf.path();
    std::string resolved = sm_.resolveImport(referer_path, str.text);

    auto sub = parseFile(resolved);
    if (!sub) {
        error(str, "failed to parse imported file `" + str.text + "`");
        return false;
    }
    // Flatten imported decls into our module.
    for (auto& s2 : sub->stmts) {
        mod.stmts.push_back(std::move(s2));
    }
    ast::ImportDecl imp;
    imp.path = str.text;
    imp.src  = {kw.range.begin, str.range.end};
    mod.stmts.emplace_back(std::move(imp));
    return true;
}

std::optional<ast::OpaqueCpp> Parser::parseBraceBlock(State& s) {
    if (s.cur.kind != Tok::LBrace) {
        error(s.cur, std::string("expected `{` (got ") +
                     tokName(s.cur.kind) + ")");
        return std::nullopt;
    }
    SourceLoc lb_begin = s.cur.range.begin;
    uint32_t  after_lb = s.cur.range.end.offset;
    // Rewind the lexer to right after the `{` (we previously prefetched
    // one or more tokens past it as part of normal lookahead).
    s.lex->seekTo(after_lb);
    Token body = s.lex->lexOpaqueBlock();
    if (body.kind == Tok::Error) return std::nullopt;
    // After lexOpaqueBlock, the closing `}` has been consumed by the
    // lexer; advance our cur to the next "real" token.
    s.cur = s.lex->next();
    ast::OpaqueCpp out;
    out.text = body.text;
    out.src  = {lb_begin, body.range.end};
    return out;
}

std::optional<ast::SignalParam> Parser::parseSignalParam(State& s) {
    Token type = expect(s, Tok::Ident, "signal parameter type");
    if (type.kind != Tok::Ident) return std::nullopt;
    Token name = expect(s, Tok::Ident, "signal parameter name");
    if (name.kind != Tok::Ident) return std::nullopt;
    ast::SignalParam p;
    p.type_name  = type.text;
    p.field_name = name.text;
    p.src        = {type.range.begin, name.range.end};
    return p;
}

bool Parser::parseElementBody(State& s, ast::ElementDecl& elem) {
    Token lb = expect(s, Tok::LBrace, "`{` to begin element body");
    if (lb.kind != Tok::LBrace) return false;

    while (s.cur.kind != Tok::RBrace && s.cur.kind != Tok::Eof) {
        if (s.cur.kind == Tok::KwState) {
            advance(s);
            auto blk = parseBraceBlock(s);
            if (!blk) return false;
            elem.state = std::move(blk);
        } else if (s.cur.kind == Tok::KwInit) {
            advance(s);
            auto blk = parseBraceBlock(s);
            if (!blk) return false;
            elem.init = std::move(blk);
        } else if (s.cur.kind == Tok::KwHandler) {
            advance(s);
            auto blk = parseBraceBlock(s);
            if (!blk) return false;
            elem.handler = std::move(blk);
        } else if (s.cur.kind == Tok::KwSignal) {
            advance(s);
            // optional typed param list:  .signal (type name, type name, ...)
            if (accept(s, Tok::LParen)) {
                while (s.cur.kind != Tok::RParen && s.cur.kind != Tok::Eof) {
                    auto p = parseSignalParam(s);
                    if (!p) return false;
                    elem.signal_params.push_back(*p);
                    if (!accept(s, Tok::Comma)) break;
                }
                expect(s, Tok::RParen, "`)`");
            }
            auto blk = parseBraceBlock(s);
            if (!blk) return false;
            elem.signal = std::move(blk);
        } else if (s.cur.kind == Tok::KwTiming) {
            // .timing { ii = N; }
            advance(s);
            Token lbrace = expect(s, Tok::LBrace, "`{` after .timing");
            if (lbrace.kind != Tok::LBrace) return false;
            while (s.cur.kind != Tok::RBrace && s.cur.kind != Tok::Eof) {
                Token id = expect(s, Tok::Ident, "timing key (e.g. `ii`)");
                if (id.kind != Tok::Ident) return false;
                Token eq = expect(s, Tok::Equals, "`=` after timing key");
                if (eq.kind != Tok::Equals) return false;
                Token val = expect(s, Tok::Integer, "integer value");
                if (val.kind != Tok::Integer) return false;
                if (id.text == "ii") {
                    try { elem.pipeline_ii = std::stoi(val.text); }
                    catch (...) { error(val, "invalid integer for ii"); return false; }
                    if (elem.pipeline_ii < 1 || elem.pipeline_ii > 64) {
                        error(val, "ii must be in [1, 64]");
                        return false;
                    }
                } else {
                    error(id, "unknown timing key `" + id.text + "` (only `ii` supported)");
                    return false;
                }
                expect(s, Tok::Semicolon, "`;` after timing entry");
            }
            expect(s, Tok::RBrace, "`}` to close .timing block");
        } else {
            error(s.cur, std::string("unexpected token in element body: ") +
                         tokName(s.cur.kind));
            return false;
        }
    }
    expect(s, Tok::RBrace, "`}`");
    return true;
}

bool Parser::parseElementDecl(State& s, ast::Module& mod) {
    Token kw = s.cur; advance(s); // .element
    Token name = expect(s, Tok::Ident, "element name");
    if (name.kind != Tok::Ident) return false;

    ast::ElementDecl decl;
    decl.name = name.text;
    decl.src  = {kw.range.begin, name.range.end};

    // Optional <in, out>
    if (accept(s, Tok::LAngle)) {
        Token in_t = expect(s, Tok::Integer, "input port count");
        if (in_t.kind != Tok::Integer) return false;
        decl.n_in_ports = static_cast<int>(in_t.int_value);
        expect(s, Tok::Comma, "`,`");
        Token out_t = expect(s, Tok::Integer, "output port count");
        if (out_t.kind != Tok::Integer) return false;
        decl.n_out_ports = static_cast<int>(out_t.int_value);
        expect(s, Tok::RAngle, "`>`");
    }

    if (!parseElementBody(s, decl)) return false;
    mod.stmts.emplace_back(std::move(decl));
    return true;
}

bool Parser::parseGroupBody(State& s, ast::ElementGroupBody& body) {
    Token lb = expect(s, Tok::LBrace, "`{` to begin element group body");
    if (lb.kind != Tok::LBrace) return false;

    while (s.cur.kind != Tok::RBrace && s.cur.kind != Tok::Eof) {
        if (!parseDeclOrChain(s, nullptr, &body)) {
            // recover
            while (s.cur.kind != Tok::RBrace && s.cur.kind != Tok::Eof &&
                   s.cur.kind != Tok::Ident && s.cur.kind != Tok::KwHost &&
                   s.cur.kind != Tok::KwVerilog) {
                advance(s);
            }
        }
    }
    expect(s, Tok::RBrace, "`}`");
    return true;
}

bool Parser::parseElementGroupDecl(State& s, ast::Module& mod) {
    Token kw = s.cur; advance(s); // .element_group
    Token name = expect(s, Tok::Ident, "element group name");
    if (name.kind != Tok::Ident) return false;

    ast::ElementGroupDecl decl;
    decl.name = name.text;
    decl.src  = {kw.range.begin, name.range.end};

    // Optional <in, out>
    if (accept(s, Tok::LAngle)) {
        Token in_t  = expect(s, Tok::Integer, "input port count");
        if (in_t.kind != Tok::Integer) return false;
        decl.n_in_ports = static_cast<int>(in_t.int_value);
        expect(s, Tok::Comma, "`,`");
        Token out_t = expect(s, Tok::Integer, "output port count");
        if (out_t.kind != Tok::Integer) return false;
        decl.n_out_ports = static_cast<int>(out_t.int_value);
        expect(s, Tok::RAngle, "`>`");
    }

    if (!parseGroupBody(s, decl.body)) return false;
    mod.stmts.emplace_back(std::move(decl));
    return true;
}

bool Parser::parseInstanceOrConnection(State& s, ast::Module& mod) {
    return parseDeclOrChain(s, &mod, nullptr);
}

bool Parser::parseHostQualifier(State& s, bool& host_ctrl, bool& autorun) {
    while (true) {
        if (accept(s, Tok::At))      { host_ctrl = true; continue; }
        if (accept(s, Tok::Amp))     { autorun   = true; continue; }
        break;
    }
    return true;
}

std::optional<ast::PortRange> Parser::parseOptionalRange(State& s) {
    if (s.cur.kind != Tok::LBracket) return std::nullopt;
    Token lb = s.cur; advance(s); // [
    ast::PortRange r;
    r.src.begin = lb.range.begin;
    Token n1 = expect(s, Tok::Integer, "port number");
    if (n1.kind != Tok::Integer) return std::nullopt;
    r.lo = static_cast<int>(n1.int_value);
    r.hi = r.lo;
    if (accept(s, Tok::Dash)) {
        Token n2 = expect(s, Tok::Integer, "port number");
        if (n2.kind != Tok::Integer) return std::nullopt;
        r.hi = static_cast<int>(n2.int_value);
    }
    if (accept(s, Tok::Star)) {
        Token nd = expect(s, Tok::Integer, "channel depth");
        if (nd.kind != Tok::Integer) return std::nullopt;
        r.depth = static_cast<int>(nd.int_value);
    }
    Token rb = expect(s, Tok::RBracket, "`]`");
    r.src.end = rb.range.end;
    return r;
}

std::optional<ast::ConnEnd> Parser::parseConnEnd(State& s) {
    // [range] node_name   OR   node_name[range]   OR   bare node
    ast::ConnEnd end;
    end.src.begin = s.cur.range.begin;

    auto leading_range = parseOptionalRange(s);
    Token name = expect(s, Tok::Ident, "node name");
    if (name.kind != Tok::Ident) return std::nullopt;
    end.node_name = name.text;
    end.src.end   = name.range.end;
    if (leading_range) {
        end.range = *leading_range;
    } else {
        // trailing range allowed, e.g. `node[2]`
        if (auto trailing = parseOptionalRange(s)) {
            end.range = *trailing;
            end.src.end = s.prev.range.end;
        }
    }
    return end;
}

// Heart of the topology grammar.
// Forms:
//   InstanceDecl:      [host|verilog] Type :: instance_name [@] [&] [( N, M [, params...] )]
//   Anonymous + chain: Node [-> | =>] Node [-> | =>] ...
//   Chain start with anonymous instantiation: Type(N,M)  -> ...
bool Parser::parseDeclOrChain(State& s,
                              ast::Module* mod_out,
                              ast::ElementGroupBody* group_out) {
    auto push_instance = [&](ast::InstanceDecl decl) {
        if (mod_out)   mod_out->stmts.emplace_back(std::move(decl));
        else if (group_out) group_out->stmts.push_back({std::move(decl)});
    };
    auto push_conn = [&](ast::ConnectionDecl conn) {
        if (mod_out)   mod_out->stmts.emplace_back(std::move(conn));
        else if (group_out) group_out->stmts.push_back({std::move(conn)});
    };

    bool prefix_host = false, prefix_verilog = false;
    if (s.cur.kind == Tok::KwHost)    { prefix_host = true;    advance(s); }
    else if (s.cur.kind == Tok::KwVerilog) { prefix_verilog = true; advance(s); }

    if (s.cur.kind != Tok::Ident) {
        error(s.cur, std::string("expected identifier (got ") +
                     tokName(s.cur.kind) + ")");
        return false;
    }

    Token first = s.cur;
    advance(s);

    // Look ahead one step.
    // - DoubleColon (`::`) means this is an InstanceDecl: `Type :: name [...]`
    // - LParen at this point means anonymous instance:    `Type(N,M)`
    // - Anything else (->, =>, ;, [, EOF, ident) means we are starting a chain
    //   from a bare node name.
    SourceLoc start = first.range.begin;

    if (s.cur.kind == Tok::DoubleColon) {
        advance(s); // ::
        Token name = expect(s, Tok::Ident, "instance name");
        if (name.kind != Tok::Ident) return false;
        ast::InstanceDecl decl;
        decl.type = first.text;
        decl.name = name.text;
        decl.src  = {start, name.range.end};
        parseHostQualifier(s, decl.host_control, decl.autorun);
        if (accept(s, Tok::LParen)) {
            // (in, out [, params...])
            Token in_t = expect(s, Tok::Integer, "input port count");
            if (in_t.kind != Tok::Integer) return false;
            decl.n_in_ports = static_cast<int>(in_t.int_value);
            expect(s, Tok::Comma, "`,`");
            // optional `*depth` after out count, e.g. `(2, 5*512)`
            Token out_t = expect(s, Tok::Integer, "output port count");
            if (out_t.kind != Tok::Integer) return false;
            decl.n_out_ports = static_cast<int>(out_t.int_value);
            if (accept(s, Tok::Star)) {
                Token d_t = expect(s, Tok::Integer, "channel depth");
                if (d_t.kind != Tok::Integer) return false;
                decl.channel_depth = static_cast<int>(d_t.int_value);
            }
            // additional params
            while (accept(s, Tok::Comma)) {
                if (s.cur.kind == Tok::Integer) {
                    ast::InstanceParam p;
                    p.kind = ast::InstanceParam::Kind::Number;
                    p.text = s.cur.text;
                    p.src  = s.cur.range;
                    decl.params.push_back(std::move(p));
                    advance(s);
                } else if (s.cur.kind == Tok::StringLit) {
                    ast::InstanceParam p;
                    p.kind = ast::InstanceParam::Kind::String;
                    p.text = s.cur.text;
                    p.src  = s.cur.range;
                    decl.params.push_back(std::move(p));
                    advance(s);
                } else {
                    error(s.cur, "expected number or string param");
                    return false;
                }
            }
            expect(s, Tok::RParen, "`)`");
        }
        accept(s, Tok::Semicolon);
        push_instance(std::move(decl));
        return true;
    }

    // Otherwise we are parsing a chain. The first node is `first`, possibly
    // with `(N,M)` indicating an anonymous instantiation, and possibly a
    // `[range]` after the node name.
    auto make_or_use_first = [&]() -> std::optional<std::string> {
        // Anonymous instance form: Type(in, out)  → produce an InstanceDecl
        // with a fresh name.
        if (s.cur.kind == Tok::LParen) {
            advance(s);
            Token in_t = expect(s, Tok::Integer, "input port count");
            if (in_t.kind != Tok::Integer) return std::nullopt;
            expect(s, Tok::Comma, "`,`");
            Token out_t = expect(s, Tok::Integer, "output port count");
            if (out_t.kind != Tok::Integer) return std::nullopt;
            int dep = 64;
            if (accept(s, Tok::Star)) {
                Token d_t = expect(s, Tok::Integer, "channel depth");
                if (d_t.kind != Tok::Integer) return std::nullopt;
                dep = static_cast<int>(d_t.int_value);
            }
            std::vector<ast::InstanceParam> params;
            while (accept(s, Tok::Comma)) {
                if (s.cur.kind == Tok::Integer) {
                    ast::InstanceParam p;
                    p.kind = ast::InstanceParam::Kind::Number;
                    p.text = s.cur.text;
                    p.src  = s.cur.range;
                    params.push_back(std::move(p));
                    advance(s);
                } else if (s.cur.kind == Tok::StringLit) {
                    ast::InstanceParam p;
                    p.kind = ast::InstanceParam::Kind::String;
                    p.text = s.cur.text;
                    p.src  = s.cur.range;
                    params.push_back(std::move(p));
                    advance(s);
                } else { break; }
            }
            expect(s, Tok::RParen, "`)`");
            ast::InstanceDecl ad;
            ad.type        = first.text;
            ad.anonymous   = true;
            ad.n_in_ports  = static_cast<int>(in_t.int_value);
            ad.n_out_ports = static_cast<int>(out_t.int_value);
            ad.channel_depth = dep;
            ad.params      = std::move(params);
            // Auto-name (caller fills name uniquely; we use position+type).
            ad.name = first.text + "__anon_" +
                      std::to_string(start.line) + "_" +
                      std::to_string(start.column);
            ad.src = {start, s.prev.range.end};
            std::string nm = ad.name;
            push_instance(std::move(ad));
            return nm;
        }
        // Named: just use the bare ident.
        return first.text;
    };

    auto first_name = make_or_use_first();
    if (!first_name) return false;

    ast::ConnEnd cur_end;
    cur_end.node_name = *first_name;
    cur_end.src       = {start, s.prev.range.end};
    if (auto r = parseOptionalRange(s)) cur_end.range = *r;

    bool any_arrow = false;
    while (s.cur.kind == Tok::Arrow || s.cur.kind == Tok::FatArrow) {
        any_arrow = true;
        bool lossy = (s.cur.kind == Tok::FatArrow);
        Token arrow = s.cur;
        advance(s);

        // Optional [range] before destination node.
        auto leading = parseOptionalRange(s);
        // Destination node:   either `Type ::` (rare in chain), or `Type(...)` anon,
        // or a bare node name (with optional trailing range).
        if (s.cur.kind != Tok::Ident && s.cur.kind != Tok::KwHost &&
            s.cur.kind != Tok::KwVerilog) {
            error(s.cur, "expected node after arrow");
            return false;
        }
        bool dst_host = false, dst_verilog = false;
        if (s.cur.kind == Tok::KwHost)    { dst_host    = true; advance(s); }
        if (s.cur.kind == Tok::KwVerilog) { dst_verilog = true; advance(s); }
        Token next = expect(s, Tok::Ident, "node name");
        if (next.kind != Tok::Ident) return false;

        std::string next_name;
        if (s.cur.kind == Tok::LParen) {
            // anonymous inline instance
            advance(s);
            Token in_t = expect(s, Tok::Integer, "input port count");
            if (in_t.kind != Tok::Integer) return false;
            expect(s, Tok::Comma, "`,`");
            Token out_t = expect(s, Tok::Integer, "output port count");
            if (out_t.kind != Tok::Integer) return false;
            int dep = 64;
            if (accept(s, Tok::Star)) {
                Token d_t = expect(s, Tok::Integer, "channel depth");
                if (d_t.kind != Tok::Integer) return false;
                dep = static_cast<int>(d_t.int_value);
            }
            std::vector<ast::InstanceParam> params;
            while (accept(s, Tok::Comma)) {
                if (s.cur.kind == Tok::Integer) {
                    ast::InstanceParam p;
                    p.kind = ast::InstanceParam::Kind::Number;
                    p.text = s.cur.text;
                    p.src  = s.cur.range;
                    params.push_back(std::move(p));
                    advance(s);
                } else if (s.cur.kind == Tok::StringLit) {
                    ast::InstanceParam p;
                    p.kind = ast::InstanceParam::Kind::String;
                    p.text = s.cur.text;
                    p.src  = s.cur.range;
                    params.push_back(std::move(p));
                    advance(s);
                } else { break; }
            }
            expect(s, Tok::RParen, "`)`");
            ast::InstanceDecl ad;
            ad.type          = next.text;
            ad.anonymous     = true;
            ad.n_in_ports    = static_cast<int>(in_t.int_value);
            ad.n_out_ports   = static_cast<int>(out_t.int_value);
            ad.channel_depth = dep;
            ad.params        = std::move(params);
            ad.name = next.text + "__anon_" +
                      std::to_string(next.range.begin.line) + "_" +
                      std::to_string(next.range.begin.column);
            ad.src = {next.range.begin, s.prev.range.end};
            next_name = ad.name;
            push_instance(std::move(ad));
        } else {
            next_name = next.text;
        }

        ast::ConnEnd dest_end;
        dest_end.node_name = next_name;
        dest_end.src       = {next.range.begin, s.prev.range.end};
        if (leading) dest_end.range = *leading;
        if (auto trailing = parseOptionalRange(s)) dest_end.range = *trailing;

        ast::ConnectionDecl c;
        c.source = cur_end;
        c.dest   = dest_end;
        c.lossy  = lossy;
        c.src    = {cur_end.src.begin, dest_end.src.end};
        push_conn(std::move(c));

        cur_end = dest_end;
    }
    accept(s, Tok::Semicolon);
    if (!any_arrow) {
        // Bare node name with no arrow — accept as a no-op (e.g. just `from_tor;`).
    }
    return true;
}

}  // namespace openclicknp
