// SPDX-License-Identifier: Apache-2.0
// SystemC runtime header — included by every generated SystemC harness.
//
// This header forwards-declares the small set of helpers the SystemC backend
// emits and reuses the same flit_t, ClSignal, port mask macros from flit.hpp
// so that user element bodies compile identically in SystemC, SW emu, and
// HLS C++.
#pragma once

#include "openclicknp/flit.hpp"

// Note: callers that include this header should already have included
// <systemc.h> if they need SystemC types. We don't include it here so that
// user code paths that don't link SystemC compile cleanly.
