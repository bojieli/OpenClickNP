// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "openclicknp/diagnostic.hpp"
#include "openclicknp/source.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace openclicknp {

enum class Tok {
    // End markers
    Eof,
    Error,

    // Punctuation
    LBrace,         // {
    RBrace,         // }
    LParen,         // (
    RParen,         // )
    LBracket,       // [
    RBracket,       // ]
    LAngle,         // <
    RAngle,         // >
    Comma,          // ,
    Semicolon,      // ;
    Colon,          // :
    DoubleColon,    // ::
    Star,           // *
    Dash,           // -
    Equals,         // =
    Dollar,         // $
    At,             // @
    Amp,            // &
    Arrow,          // ->
    FatArrow,       // =>

    // Keywords
    KwImport,
    KwElement,
    KwElementGroup,
    KwState,
    KwInit,
    KwHandler,
    KwSignal,
    KwTiming,
    KwRepeat,
    KwBreak,
    KwConstexpr,
    KwHost,
    KwVerilog,
    KwPortAny,

    // Literals
    Ident,
    Integer,        // decimal, hex (0x...), or octal (0...)
    StringLit,      // "..."

    // Composite: opaque C++ block (the body of .state/.init/.handler/.signal)
    OpaqueCpp,
};

struct Token {
    Tok          kind = Tok::Eof;
    std::string  text;        // for ident, integer, stringlit, opaque C++
    SourceRange  range;
    uint64_t     int_value = 0;   // for Integer (best-effort)
};

class Lexer {
public:
    Lexer(const SourceBuffer& buf, uint32_t file_id, DiagnosticEngine& diags);

    // Next token. After Eof, repeats Eof.
    Token next();

    // Special form: lex an opaque C++ block starting at the current position
    // (the lexer must have just returned `{`; this consumes balanced braces
    // until the matching `}` and returns the inner text as one OpaqueCpp
    // token; it does NOT include the surrounding braces).
    //
    // Brace counting respects /* */, // \n, "..." with escapes, and '...'
    // character literals.
    Token lexOpaqueBlock();

    // Reset the lexer to a known byte offset. Used by the parser after
    // pre-fetching past `{` to rewind into opaque-block mode.
    void seekTo(uint32_t offset);

private:
    const SourceBuffer& buf_;
    const std::string&  src_;
    uint32_t            file_id_;
    DiagnosticEngine&   diags_;

    size_t   pos_     = 0;
    uint32_t line_    = 1;
    uint32_t col_     = 1;

    // Helpers
    void     skipWhitespaceAndComments();
    bool     atEnd() const { return pos_ >= src_.size(); }
    char     peek(size_t k = 0) const {
        return (pos_ + k < src_.size()) ? src_[pos_ + k] : '\0';
    }
    void     advance();
    SourceLoc here() const;

    Token    tokOne(Tok kind, char c, SourceLoc start);
    Token    tokTwo(Tok kind, SourceLoc start);
    Token    lexIdentOrKeyword(SourceLoc start);
    Token    lexNumber(SourceLoc start);
    Token    lexString(SourceLoc start);
    Token    lexDirectiveKeyword(SourceLoc start);
};

const char* tokName(Tok t);

}  // namespace openclicknp
