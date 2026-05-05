// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/diagnostic.hpp"
#include "openclicknp/lexer.hpp"
#include "openclicknp/source.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace openclicknp;

static std::vector<Tok> lex_all(const std::string& src) {
    SourceManager sm;
    uint32_t fid = sm.addBuffer("<inline>", src);
    DiagnosticEngine d(sm);
    Lexer lex(sm.buffer(fid), fid, d);
    std::vector<Tok> out;
    while (true) {
        Token t = lex.next();
        out.push_back(t.kind);
        if (t.kind == Tok::Eof || t.kind == Tok::Error) break;
    }
    return out;
}

int main() {
    {
        auto t = lex_all("import \"x.clnp\";");
        assert(t == std::vector<Tok>({Tok::KwImport, Tok::StringLit,
                                      Tok::Semicolon, Tok::Eof}));
    }
    {
        auto t = lex_all(".element Pass <1, 1>");
        assert(t == std::vector<Tok>({Tok::KwElement, Tok::Ident,
                                      Tok::LAngle, Tok::Integer, Tok::Comma,
                                      Tok::Integer, Tok::RAngle, Tok::Eof}));
    }
    {
        auto t = lex_all("a -> b => c[1*128]");
        assert(t == std::vector<Tok>({Tok::Ident, Tok::Arrow, Tok::Ident,
                                      Tok::FatArrow, Tok::Ident,
                                      Tok::LBracket, Tok::Integer, Tok::Star,
                                      Tok::Integer, Tok::RBracket, Tok::Eof}));
    }
    {
        // Numbers: hex / decimal
        SourceManager sm;
        uint32_t fid = sm.addBuffer("<inline>", "0x1f 42");
        DiagnosticEngine d(sm);
        Lexer lex(sm.buffer(fid), fid, d);
        Token a = lex.next(); assert(a.kind == Tok::Integer && a.int_value == 31);
        Token b = lex.next(); assert(b.kind == Tok::Integer && b.int_value == 42);
    }
    std::printf("lexer tests: OK\n");
    return 0;
}
