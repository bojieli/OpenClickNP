// SPDX-License-Identifier: Apache-2.0
//
// Minimal multi-precision integer arithmetic for RSA / modexp pipelines.
//
// All operations work on fixed-width N-limb numbers (each limb = 64 bits,
// little-endian: limbs[0] is the least significant). Designed to be:
//   • plain C++17 (no STL allocators in the hot path), so the SW emulator
//     compiles and runs identically;
//   • HLS-friendly (no recursion, no virtuals, fixed-size arrays, no
//     dynamic allocation);
//   • parameterized by limb count, so the same code handles 1024, 2048,
//     and 4096-bit operands.
//
// Reference: standard textbook schoolbook multiply + CIOS Montgomery
// reduction (Koc, Acar, Kaliski 1996, "Analyzing and Comparing Montgomery
// Multiplication Algorithms"). No code copied — implementation written
// from the algorithm description.
#pragma once

#include <array>
#include <cstdint>
#include <cstddef>

namespace openclicknp::bigint {

constexpr int LIMB_BITS = 64;

// 64-bit-limbed fixed-width integer. limbs[0] = least significant.
template <int N_LIMBS>
struct U {
    std::array<uint64_t, N_LIMBS> limbs{};

    void set_zero() { for (int i = 0; i < N_LIMBS; ++i) limbs[i] = 0; }
    bool is_zero() const {
        for (int i = 0; i < N_LIMBS; ++i) if (limbs[i]) return false;
        return true;
    }
    bool is_odd() const { return (limbs[0] & 1u) != 0; }

    // bit i of the number (i=0 is LSB).
    bool bit(int i) const {
        if (i < 0 || i >= N_LIMBS * LIMB_BITS) return false;
        return ((limbs[i / LIMB_BITS] >> (i % LIMB_BITS)) & 1u) != 0;
    }

    // Most significant set-bit position (for early-exit on modexp loop).
    int bitlen() const {
        for (int i = N_LIMBS - 1; i >= 0; --i) {
            if (limbs[i] == 0) continue;
            for (int b = LIMB_BITS - 1; b >= 0; --b) {
                if ((limbs[i] >> b) & 1u) return i * LIMB_BITS + b + 1;
            }
        }
        return 0;
    }
};

// 128-bit ops via __uint128_t where available — HLS recognizes this and
// maps it to 64×64-bit DSP-backed multipliers.
using u128 = unsigned __int128;

// Compare a vs b. Returns -1 / 0 / +1.
template <int N>
inline int cmp(const U<N>& a, const U<N>& b) {
    for (int i = N - 1; i >= 0; --i) {
        if (a.limbs[i] != b.limbs[i]) return a.limbs[i] < b.limbs[i] ? -1 : 1;
    }
    return 0;
}

// out = a + b, returns carry-out.
template <int N>
inline uint64_t add(U<N>& out, const U<N>& a, const U<N>& b) {
    uint64_t carry = 0;
    for (int i = 0; i < N; ++i) {
        u128 s = (u128)a.limbs[i] + (u128)b.limbs[i] + carry;
        out.limbs[i] = static_cast<uint64_t>(s);
        carry = static_cast<uint64_t>(s >> 64);
    }
    return carry;
}

// out = a - b, returns borrow-out (1 if a < b).
template <int N>
inline uint64_t sub(U<N>& out, const U<N>& a, const U<N>& b) {
    uint64_t borrow = 0;
    for (int i = 0; i < N; ++i) {
        u128 d = (u128)a.limbs[i] - (u128)b.limbs[i] - borrow;
        out.limbs[i] = static_cast<uint64_t>(d);
        borrow = (d >> 64) ? 1u : 0u;
    }
    return borrow;
}

// Schoolbook multiply: out (2N limbs) = a (N) * b (N).
template <int N>
inline void mul(U<2 * N>& out, const U<N>& a, const U<N>& b) {
    out.set_zero();
    for (int i = 0; i < N; ++i) {
        uint64_t carry = 0;
        for (int j = 0; j < N; ++j) {
            u128 prod = (u128)a.limbs[i] * (u128)b.limbs[j]
                        + (u128)out.limbs[i + j] + (u128)carry;
            out.limbs[i + j] = static_cast<uint64_t>(prod);
            carry = static_cast<uint64_t>(prod >> 64);
        }
        out.limbs[i + N] = carry;
    }
}

// CIOS Montgomery multiplication: out = a · b · R^(-1) mod n.
//
// Reference: Koç-Acar-Kaliski 1996, "Analyzing and Comparing Montgomery
// Multiplication Algorithms", Algorithm CIOS (Coarsely Integrated
// Operand Scanning). Uses an N+2-limb accumulator to absorb the worst
// case of two carries per iteration without overflowing.
//
// Result is in [0, 2n); we do a final conditional subtract to reduce
// into [0, n).
template <int N>
inline void mont_mul(U<N>& out, const U<N>& a, const U<N>& b,
                     const U<N>& n, uint64_t n_inv) {
    // Accumulator needs N+2 limbs: N for the working result, 1 for the
    // outer multiply carry, 1 for the reduction's add-back overflow.
    uint64_t t[N + 2] = {};

    for (int i = 0; i < N; ++i) {
        // Step 1: t += a[i] * b
        uint64_t carry = 0;
        for (int j = 0; j < N; ++j) {
            u128 s = (u128)a.limbs[i] * (u128)b.limbs[j]
                     + (u128)t[j] + (u128)carry;
            t[j] = static_cast<uint64_t>(s);
            carry = static_cast<uint64_t>(s >> 64);
        }
        u128 s_top = (u128)t[N] + (u128)carry;
        t[N] = static_cast<uint64_t>(s_top);
        t[N + 1] += static_cast<uint64_t>(s_top >> 64);

        // Step 2: m = t[0] * n_inv (mod 2^64); t = (t + m * n) / 2^64
        uint64_t m = t[0] * n_inv;
        carry = 0;
        for (int j = 0; j < N; ++j) {
            u128 s = (u128)m * (u128)n.limbs[j]
                     + (u128)t[j] + (u128)carry;
            t[j] = static_cast<uint64_t>(s);
            carry = static_cast<uint64_t>(s >> 64);
        }
        u128 s2 = (u128)t[N] + (u128)carry;
        t[N] = static_cast<uint64_t>(s2);
        t[N + 1] += static_cast<uint64_t>(s2 >> 64);

        // After this, t[0] is 0 by construction. Shift right by one limb.
        for (int j = 0; j < N + 1; ++j) t[j] = t[j + 1];
        t[N + 1] = 0;
    }

    // out = t mod n. After CIOS, t fits in N+1 limbs and is in [0, 2n).
    // If the (N+1)-th limb is non-zero or t >= n, subtract n.
    for (int i = 0; i < N; ++i) out.limbs[i] = t[i];
    if (t[N] != 0 || cmp(out, n) >= 0) {
        U<N> tmp; sub(tmp, out, n); out = tmp;
    }
}

// Compute n_inv = -n^(-1) mod 2^64 by Newton-Hensel lifting.
// Requires n.limbs[0] to be odd (it is, for any RSA modulus).
template <int N>
inline uint64_t compute_n_inv(const U<N>& n) {
    uint64_t n0 = n.limbs[0];
    // Hensel lifting: x = 1 - x*n  doubles correct bits each iteration.
    uint64_t x = 1;
    for (int i = 0; i < 6; ++i) {       // 2 -> 4 -> 8 -> 16 -> 32 -> 64
        x = x * (2u - n0 * x);
    }
    // Now x = n^(-1) mod 2^64. Negate to get -n^(-1).
    return 0u - x;
}

// Compute R mod n where R = 2^(N·LIMB_BITS).
//
// Algorithm: bit-serial shift-and-reduce. Start with x = 1, then double
// (shift-left by 1) and subtract n if x >= n, repeated N·LIMB_BITS times.
// After N·64 doublings, x = 2^(N·64) mod n = R mod n. O(N·64) iterations,
// each constant-time.
template <int N>
inline void compute_R_mod_n(U<N>& out, const U<N>& n) {
    out.set_zero();
    out.limbs[0] = 1;
    for (int step = 0; step < N * LIMB_BITS; ++step) {
        // Shift left by 1.
        uint64_t carry = 0;
        for (int i = 0; i < N; ++i) {
            uint64_t hi = out.limbs[i] >> (LIMB_BITS - 1);
            out.limbs[i] = (out.limbs[i] << 1) | carry;
            carry = hi;
        }
        // After shift, carry is the bit that fell off the top — if it's
        // 1, we know x >= 2^(N·64), which is > n (assuming n < 2^(N·64));
        // subtract n once. If x is still >= n, subtract again.
        if (carry || cmp(out, n) >= 0) {
            U<N> t; sub(t, out, n); out = t;
        }
        if (cmp(out, n) >= 0) {
            U<N> t; sub(t, out, n); out = t;
        }
    }
}

// R^2 mod n, used to convert into Montgomery form: x_mont = mont_mul(x, R^2).
//
// Computed directly by bit-serial shift-and-reduce: 2N·64 doublings of 1
// give 2^(2N·64) mod n = R^2 mod n. Naively going through mont_mul(R, R)
// would yield R · R · R^(-1) = R, not R^2 — that's not what we want.
template <int N>
inline void compute_R2_mod_n(U<N>& out, const U<N>& n, uint64_t /*n_inv*/) {
    out.set_zero();
    out.limbs[0] = 1;
    for (int step = 0; step < 2 * N * LIMB_BITS; ++step) {
        uint64_t carry = 0;
        for (int i = 0; i < N; ++i) {
            uint64_t hi = out.limbs[i] >> (LIMB_BITS - 1);
            out.limbs[i] = (out.limbs[i] << 1) | carry;
            carry = hi;
        }
        if (carry || cmp(out, n) >= 0) {
            U<N> t; sub(t, out, n); out = t;
        }
        if (cmp(out, n) >= 0) {
            U<N> t; sub(t, out, n); out = t;
        }
    }
}

// Modular exponentiation: out = m^e mod n.
//
// Uses left-to-right square-and-multiply with Montgomery multiplication.
// All intermediates live in Montgomery form (x_mont = x · R mod n).
template <int N>
inline void modexp(U<N>& out, const U<N>& m, const U<N>& e, const U<N>& n) {
    uint64_t n_inv = compute_n_inv(n);

    // Convert m into Montgomery form: m_mont = m · R mod n
    U<N> R2; compute_R2_mod_n(R2, n, n_inv);
    U<N> m_mont; mont_mul(m_mont, m, R2, n, n_inv);

    // result_mont starts as 1 in Montgomery form = R mod n.
    U<N> r_mont; compute_R_mod_n(r_mont, n);

    int blen = e.bitlen();
    for (int i = blen - 1; i >= 0; --i) {
        // square
        U<N> sq; mont_mul(sq, r_mont, r_mont, n, n_inv);
        r_mont = sq;
        if (e.bit(i)) {
            U<N> mu; mont_mul(mu, r_mont, m_mont, n, n_inv);
            r_mont = mu;
        }
    }

    // Convert out of Montgomery form: result = result_mont · 1 · R^(-1).
    U<N> one; one.set_zero(); one.limbs[0] = 1;
    mont_mul(out, r_mont, one, n, n_inv);
}

}  // namespace openclicknp::bigint
