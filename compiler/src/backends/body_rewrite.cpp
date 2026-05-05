// SPDX-License-Identifier: Apache-2.0
#include "body_rewrite.hpp"

#include <cctype>
#include <sstream>

namespace openclicknp::backends {

namespace {
bool isIdentChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}
}  // namespace

std::pair<std::string, std::string> splitState(const std::string& body) {
    std::ostringstream consts, members;
    std::istringstream is(body);
    std::string line;
    while (std::getline(is, line)) {
        size_t s = 0;
        while (s < line.size() &&
               std::isspace(static_cast<unsigned char>(line[s]))) ++s;
        std::string tail = line.substr(s);
        bool is_constexpr = false;
        if (tail.rfind("constexpr ", 0) == 0) {
            is_constexpr = true;
        } else if (tail.rfind("static constexpr ", 0) == 0) {
            is_constexpr = true;
            tail = tail.substr(std::string("static ").size());
        }
        if (is_constexpr) {
            consts << "    static " << tail << "\n";
        } else {
            members << line << "\n";
        }
    }
    return {consts.str(), members.str()};
}

std::string rewriteReturns(const std::string& in, const std::string& which) {
    std::string out;
    out.reserve(in.size() + 64);
    size_t i = 0;

    auto skip_string = [&](char q) {
        out.push_back(in[i]);
        ++i;
        while (i < in.size() && in[i] != q) {
            if (in[i] == '\\' && i + 1 < in.size()) {
                out.push_back(in[i]); out.push_back(in[i+1]);
                i += 2;
            } else {
                out.push_back(in[i++]);
            }
        }
        if (i < in.size()) { out.push_back(in[i++]); }
    };

    while (i < in.size()) {
        char c = in[i];
        // Pass through comments and string/char literals untouched.
        if (c == '/' && i + 1 < in.size() && in[i+1] == '/') {
            while (i < in.size() && in[i] != '\n') out.push_back(in[i++]);
            continue;
        }
        if (c == '/' && i + 1 < in.size() && in[i+1] == '*') {
            out.push_back(in[i++]);
            out.push_back(in[i++]);
            while (i < in.size() && !(in[i] == '*' && i + 1 < in.size() && in[i+1] == '/')) {
                out.push_back(in[i++]);
            }
            if (i + 1 < in.size()) { out.push_back(in[i++]); out.push_back(in[i++]); }
            continue;
        }
        if (c == '"' || c == '\'') { skip_string(c); continue; }

        // Match standalone `return`.
        const std::string kw = "return";
        if (in.compare(i, kw.size(), kw) == 0 &&
            (i == 0 || !isIdentChar(in[i-1])) &&
            (i + kw.size() < in.size()) && !isIdentChar(in[i + kw.size()])) {
            // Capture up to the next top-level `;` (avoiding ones inside parens).
            size_t j = i + kw.size();
            int paren = 0;
            std::string expr;
            while (j < in.size()) {
                char d = in[j];
                if (d == '(') { paren++; expr.push_back(d); ++j; continue; }
                if (d == ')') { paren--; expr.push_back(d); ++j; continue; }
                if (d == ';' && paren == 0) break;
                expr.push_back(d);
                ++j;
            }
            // Trim leading whitespace
            size_t s = 0;
            while (s < expr.size() && std::isspace(static_cast<unsigned char>(expr[s]))) ++s;
            std::string e = expr.substr(s);
            if (e.empty()) e = "openclicknp::PORT_ALL";
            out += "{ _ret = (";
            out += e;
            out += "); goto _end_";
            out += which;
            out += "; }";
            i = j + 1;        // skip the `;`
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

}  // namespace openclicknp::backends
