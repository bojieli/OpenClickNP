// SPDX-License-Identifier: Apache-2.0
//
// Helpers shared across backends that emit the user element bodies.
//
// rewriteReturns: in handler/signal/init bodies, replaces every standalone
// `return EXPR;` with `{ _ret = (EXPR); goto _end_<which>; }`.
//
// splitState: divide a `.state { ... }` body into two strings:
//   1. `[static] constexpr T NAME = EXPR;` declarations — these are
//      lifted to function scope so HLS local structs accept them.
//   2. Everything else — these become struct members.
// HLS C++ forbids static members in local-scope structs, so we hoist the
// constants out and let the struct's array-size declarations reference
// them at function scope.
#pragma once

#include <string>
#include <utility>

namespace openclicknp::backends {

std::string rewriteReturns(const std::string& body, const std::string& which);

// Returns {constexpr_lines, struct_member_lines}. Both strings are
// newline-terminated. constexpr_lines have any user-written `static`
// removed (they will be re-added by the caller as `static constexpr`
// at function scope).
std::pair<std::string, std::string> splitState(const std::string& body);

}  // namespace openclicknp::backends
