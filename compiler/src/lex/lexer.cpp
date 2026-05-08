// SPDX-License-Identifier: Apache-2.0
#include "openclicknp/lexer.hpp"

#include <cctype>
#include <unordered_map>

namespace openclicknp {

const char* tokName(Tok t) {
    switch (t) {
        case Tok::Eof:            return "<eof>";
        case Tok::Error:          return "<error>";
        case Tok::LBrace:         return "{";
        case Tok::RBrace:         return "}";
        case Tok::LParen:         return "(";
        case Tok::RParen:         return ")";
        case Tok::LBracket:       return "[";
        case Tok::RBracket:       return "]";
        case Tok::LAngle:         return "<";
        case Tok::RAngle:         return ">";
        case Tok::Comma:          return ",";
        case Tok::Semicolon:      return ";";
        case Tok::Colon:          return ":";
        case Tok::DoubleColon:    return "::";
        case Tok::Star:           return "*";
        case Tok::Dash:           return "-";
        case Tok::Equals:         return "=";
        case Tok::Dollar:         return "$";
        case Tok::At:             return "@";
        case Tok::Amp:            return "&";
        case Tok::Arrow:          return "->";
        case Tok::FatArrow:       return "=>";
        case Tok::KwImport:       return "import";
        case Tok::KwElement:      return ".element";
        case Tok::KwElementGroup: return ".element_group";
        case Tok::KwState:        return ".state";
        case Tok::KwInit:         return ".init";
        case Tok::KwHandler:      return ".handler";
        case Tok::KwSignal:       return ".signal";
        case Tok::KwTiming:       return ".timing";
        case Tok::KwRepeat:       return ".repeat";
        case Tok::KwBreak:        return "BREAK";
        case Tok::KwConstexpr:    return "constexpr";
        case Tok::KwHost:         return "host";
        case Tok::KwVerilog:      return "verilog";
        case Tok::KwPortAny:      return "PORT_ANY";
        case Tok::Ident:          return "identifier";
        case Tok::Integer:        return "integer";
        case Tok::StringLit:      return "string";
        case Tok::OpaqueCpp:      return "<opaque-cpp-block>";
    }
    return "?";
}

Lexer::Lexer(const SourceBuffer& buf, uint32_t file_id, DiagnosticEngine& diags)
    : buf_(buf), src_(buf.contents()), file_id_(file_id), diags_(diags) {}

void Lexer::seekTo(uint32_t offset) {
    if (offset > src_.size()) offset = static_cast<uint32_t>(src_.size());
    auto [ln, cl] = buf_.lineColOf(offset);
    pos_  = offset;
    line_ = ln;
    col_  = cl;
}

SourceLoc Lexer::here() const {
    return SourceLoc{file_id_, line_, col_, static_cast<uint32_t>(pos_)};
}

void Lexer::advance() {
    if (atEnd()) return;
    char c = src_[pos_++];
    if (c == '\n') { ++line_; col_ = 1; }
    else           { ++col_; }
}

void Lexer::skipWhitespaceAndComments() {
    while (!atEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            while (!atEnd() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            advance(); advance();
            while (!atEnd() && !(peek() == '*' && peek(1) == '/')) advance();
            if (!atEnd()) { advance(); advance(); }
        } else if (c == '#') {
            // Line comment (legacy script-style)
            while (!atEnd() && peek() != '\n') advance();
        } else {
            break;
        }
    }
}

Token Lexer::tokOne(Tok kind, char /*c*/, SourceLoc start) {
    advance();
    Token t;
    t.kind  = kind;
    t.range = {start, here()};
    return t;
}

Token Lexer::tokTwo(Tok kind, SourceLoc start) {
    advance(); advance();
    Token t;
    t.kind  = kind;
    t.range = {start, here()};
    return t;
}

Token Lexer::lexIdentOrKeyword(SourceLoc start) {
    size_t b = pos_;
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) ||
                        peek() == '_')) {
        advance();
    }
    std::string text = src_.substr(b, pos_ - b);

    static const std::unordered_map<std::string, Tok> kw = {
        {"import",    Tok::KwImport},
        {"BREAK",     Tok::KwBreak},
        {"constexpr", Tok::KwConstexpr},
        {"host",      Tok::KwHost},
        {"verilog",   Tok::KwVerilog},
        {"PORT_ANY",  Tok::KwPortAny},
    };
    Token t;
    auto it = kw.find(text);
    t.kind  = (it == kw.end()) ? Tok::Ident : it->second;
    t.text  = std::move(text);
    t.range = {start, here()};
    return t;
}

Token Lexer::lexDirectiveKeyword(SourceLoc start) {
    // Already at '.' ; consume it then identifier and dispatch.
    advance(); // consume '.'
    size_t b = pos_;
    while (!atEnd() && (std::isalnum(static_cast<unsigned char>(peek())) ||
                        peek() == '_')) {
        advance();
    }
    std::string name = src_.substr(b, pos_ - b);

    static const std::unordered_map<std::string, Tok> dir = {
        {"element",       Tok::KwElement},
        {"element_group", Tok::KwElementGroup},
        {"state",         Tok::KwState},
        {"init",          Tok::KwInit},
        {"handler",       Tok::KwHandler},
        {"signal",        Tok::KwSignal},
        {"timing",        Tok::KwTiming},
        {"repeat",        Tok::KwRepeat},
    };
    Token t;
    auto it = dir.find(name);
    if (it == dir.end()) {
        t.kind = Tok::Error;
        t.text = "." + name;
        t.range = {start, here()};
        diags_.error(t.range, "unknown directive `." + name + "`");
        return t;
    }
    t.kind  = it->second;
    t.text  = "." + name;
    t.range = {start, here()};
    return t;
}

Token Lexer::lexNumber(SourceLoc start) {
    size_t b = pos_;
    int base = 10;
    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        advance(); advance();
        b = pos_;
        base = 16;
        while (!atEnd() && std::isxdigit(static_cast<unsigned char>(peek())))
            advance();
    } else if (peek() == '0' &&
               peek(1) >= '0' && peek(1) <= '7') {
        advance(); // consume leading 0
        b = pos_;
        base = 8;
        while (!atEnd() && peek() >= '0' && peek() <= '7') advance();
    } else {
        while (!atEnd() && std::isdigit(static_cast<unsigned char>(peek())))
            advance();
    }
    std::string num_text = src_.substr(b, pos_ - b);
    Token t;
    t.kind = Tok::Integer;
    t.text = num_text;
    try {
        t.int_value = std::stoull(num_text, nullptr, base);
    } catch (...) {
        t.int_value = 0;
    }
    t.range = {start, here()};
    return t;
}

Token Lexer::lexString(SourceLoc start) {
    advance(); // opening "
    std::string out;
    while (!atEnd() && peek() != '"') {
        char c = peek();
        if (c == '\\') {
            advance();
            char esc = peek();
            switch (esc) {
                case 'n':  out.push_back('\n'); break;
                case 't':  out.push_back('\t'); break;
                case 'r':  out.push_back('\r'); break;
                case '\\': out.push_back('\\'); break;
                case '"':  out.push_back('"');  break;
                case '0':  out.push_back('\0'); break;
                default:   out.push_back(esc);  break;
            }
            advance();
        } else {
            out.push_back(c);
            advance();
        }
    }
    Token t;
    t.kind = Tok::StringLit;
    t.text = std::move(out);
    if (atEnd()) {
        t.kind = Tok::Error;
        diags_.error({start, here()}, "unterminated string literal");
    } else {
        advance(); // closing "
    }
    t.range = {start, here()};
    return t;
}

Token Lexer::next() {
    skipWhitespaceAndComments();
    SourceLoc start = here();
    if (atEnd()) {
        Token t; t.kind = Tok::Eof; t.range = {start, start}; return t;
    }
    char c = peek();

    // Multi-char punctuation first.
    if (c == '-' && peek(1) == '>') return tokTwo(Tok::Arrow, start);
    if (c == '=' && peek(1) == '>') return tokTwo(Tok::FatArrow, start);
    if (c == ':' && peek(1) == ':') return tokTwo(Tok::DoubleColon, start);

    switch (c) {
        case '{': return tokOne(Tok::LBrace,    c, start);
        case '}': return tokOne(Tok::RBrace,    c, start);
        case '(': return tokOne(Tok::LParen,    c, start);
        case ')': return tokOne(Tok::RParen,    c, start);
        case '[': return tokOne(Tok::LBracket,  c, start);
        case ']': return tokOne(Tok::RBracket,  c, start);
        case '<': return tokOne(Tok::LAngle,    c, start);
        case '>': return tokOne(Tok::RAngle,    c, start);
        case ',': return tokOne(Tok::Comma,     c, start);
        case ';': return tokOne(Tok::Semicolon, c, start);
        case ':': return tokOne(Tok::Colon,     c, start);
        case '*': return tokOne(Tok::Star,      c, start);
        case '-': return tokOne(Tok::Dash,      c, start);
        case '=': return tokOne(Tok::Equals,    c, start);
        case '$': return tokOne(Tok::Dollar,    c, start);
        case '@': return tokOne(Tok::At,        c, start);
        case '&': return tokOne(Tok::Amp,       c, start);
        case '"': return lexString(start);
        default: break;
    }
    if (c == '.') {
        return lexDirectiveKeyword(start);
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return lexNumber(start);
    }
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return lexIdentOrKeyword(start);
    }

    // Unknown character.
    Token t;
    t.kind = Tok::Error;
    t.text = std::string(1, c);
    advance();
    t.range = {start, here()};
    diags_.error(t.range, std::string("unexpected character `") + c + "`");
    return t;
}

Token Lexer::lexOpaqueBlock() {
    // Caller has just consumed '{' and we are now positioned on the first
    // character of the body.
    SourceLoc start = here();
    size_t b = pos_;
    int depth = 1;

    auto skipString = [&](char q) {
        advance(); // opening quote
        while (!atEnd() && peek() != q) {
            if (peek() == '\\' && pos_ + 1 < src_.size()) {
                advance(); advance();
            } else {
                advance();
            }
        }
        if (!atEnd()) advance(); // closing quote
    };

    while (!atEnd() && depth > 0) {
        char c = peek();
        if (c == '/' && peek(1) == '/') {
            while (!atEnd() && peek() != '\n') advance();
        } else if (c == '/' && peek(1) == '*') {
            advance(); advance();
            while (!atEnd() && !(peek() == '*' && peek(1) == '/')) advance();
            if (!atEnd()) { advance(); advance(); }
        } else if (c == '"' || c == '\'') {
            skipString(c);
        } else if (c == '{') {
            ++depth;
            advance();
        } else if (c == '}') {
            --depth;
            if (depth == 0) break;
            advance();
        } else {
            advance();
        }
    }
    Token t;
    t.kind = Tok::OpaqueCpp;
    if (depth != 0) {
        diags_.error({start, here()}, "unterminated `{` block");
        t.kind = Tok::Error;
    }
    t.text  = src_.substr(b, pos_ - b);
    t.range = {start, here()};
    if (peek() == '}') advance(); // consume closing brace, kept off the text
    return t;
}

}  // namespace openclicknp
