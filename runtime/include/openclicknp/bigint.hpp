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
//
// HLS notes: when synthesized, we mark this function INLINE off so
// HLS allocates a single shared instance instead of replicating the
// body at each call site (which made the design balloon to millions
// of instructions before the pragmas were added). The N+2-limb local
// array becomes a registered work buffer; the inner loops pipeline
// naturally at II=1.
template <int N>
inline void mont_mul(U<N>& out, const U<N>& a, const U<N>& b,
                     const U<N>& n, uint64_t n_inv) {
#ifdef __SYNTHESIS__
#pragma HLS INLINE off
#endif
    // Accumulator needs N+2 limbs: N for the working result, 1 for the
    // outer multiply carry, 1 for the reduction's add-back overflow.
    uint64_t t[N + 2] = {};

    for (int i = 0; i < N; ++i) {
#ifdef __SYNTHESIS__
#pragma HLS PIPELINE II=1
#endif
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

// Constant-time conditional copy: dst = cond ? src : dst, where `cond`
// must be 0 or 1. The branch is encoded as an arithmetic mask so the
// compiler can't lower it to a data-dependent jump.
template <int N>
inline void const_time_select(U<N>& dst, const U<N>& src, uint64_t cond) {
    uint64_t mask = ~(cond - 1);   // cond=1 → 0xFF…FF, cond=0 → 0
    for (int i = 0; i < N; ++i)
        dst.limbs[i] = (dst.limbs[i] & ~mask) | (src.limbs[i] & mask);
}

// Constant-time conditional swap: if cond==1 swap a and b, else leave.
// Standard XOR-mask trick — no data-dependent branch.
template <int N>
inline void const_time_swap(U<N>& a, U<N>& b, uint64_t cond) {
    uint64_t mask = ~(cond - 1);
    for (int i = 0; i < N; ++i) {
        uint64_t diff = (a.limbs[i] ^ b.limbs[i]) & mask;
        a.limbs[i] ^= diff;
        b.limbs[i] ^= diff;
    }
}

// Constant-time modexp via the Montgomery powering ladder, in the
// Joye-Yen (CHES 2002) form with a single constant-time swap per bit.
//
// Per-iteration cost is exactly TWO Montgomery multiplications (one
// product + one square), regardless of the bit value. The earlier
// implementation here computed all four candidate states and masked
// the selection — correct, but used 4 mont_muls/bit. The Joye-Yen
// form has been the standard since the 2002 paper.
//
// Reference (algorithm only — implementation written from spec):
//   Joye, M. and Yen, S.M., "The Montgomery Powering Ladder",
//   Cryptographic Hardware and Embedded Systems (CHES) 2002.
template <int N>
inline void modexp_consttime(U<N>& out,
                             const U<N>& m,
                             const U<N>& e,
                             const U<N>& n) {
    uint64_t n_inv = compute_n_inv(n);
    U<N> R2; compute_R2_mod_n(R2, n, n_inv);

    // Operand into Montgomery form.
    U<N> m_mont; mont_mul(m_mont, m, R2, n, n_inv);

    // Ladder state: R0 = 1·R, R1 = m·R (Montgomery form).
    U<N> R0; compute_R_mod_n(R0, n);
    U<N> R1 = m_mont;

    // Joye-Yen with cswap:
    //   for each bit (MSB → LSB):
    //     cswap(R0, R1, bit)
    //     R1 = R0 * R1                  (mont_mul #1)
    //     R0 = R0^2                     (mont_mul #2)
    //     cswap(R0, R1, bit)
    // After the loop, R0 holds the result (R · m^e mod n).
    for (int i = N * LIMB_BITS - 1; i >= 0; --i) {
        uint64_t bit = (e.limbs[i / LIMB_BITS] >> (i % LIMB_BITS)) & 1u;

        const_time_swap(R0, R1, bit);
        U<N> prod;  mont_mul(prod, R0, R1, n, n_inv);
        U<N> sq;    mont_mul(sq,   R0, R0, n, n_inv);
        R1 = prod;
        R0 = sq;
        const_time_swap(R0, R1, bit);
    }

    // Convert R0 back out of Montgomery form.
    U<N> one; one.set_zero(); one.limbs[0] = 1;
    mont_mul(out, R0, one, n, n_inv);
}

// Modular exponentiation: out = m^e mod n.
//
// Uses left-to-right square-and-multiply with Montgomery multiplication.
// All intermediates live in Montgomery form (x_mont = x · R mod n).
//
// **Not constant-time** — the `if (e.bit(i))` branches on the secret
// exponent. Use modexp_consttime() above for any private-key path.
//
// HLS note: the bitlen-driven outer loop is data-dependent and won't
// pipeline cleanly. The recommended HLS path is to drive modexp from
// a multi-cycle .handler state machine that calls mont_mul once per
// invocation (see elements/crypto/RSA_ModExp_<BITS>.clnp). This
// inline definition is the SW-emu reference.
template <int N>
inline void modexp(U<N>& out, const U<N>& m, const U<N>& e, const U<N>& n) {
#ifdef __SYNTHESIS__
#pragma HLS INLINE off
#endif
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

// CRT-accelerated RSA private-key operation: out = c^d mod n, computed
// via the Chinese Remainder Theorem with the half-bit-width primes p, q.
//
// Inputs (all M = N/2 limbs):
//   c  — ciphertext (N limbs, but treated mod p / mod q after reduction)
//   p, q — primes (M limbs each), with n = p*q (caller's responsibility)
//   dp = d mod (p-1), dq = d mod (q-1)
//   qInv = q^(-1) mod p
//
// Output (N limbs): m = c^d mod n.
//
// Uses constant-time modexp on each half — appropriate because dp, dq
// are secret. Reference: PKCS#1 v2.2 §5.1.2.
template <int N, int M>
inline void rsa_crt_decrypt(U<N>& out,
                            const U<N>& c,
                            const U<M>& p,
                            const U<M>& q,
                            const U<M>& dp,
                            const U<M>& dq,
                            const U<M>& qInv) {
    static_assert(M * 2 == N, "rsa_crt_decrypt requires M*2 == N");

    // Reduce N-limb src to M-limb dst (mod the M-limb modulus) via
    // bit-serial shift-and-subtract: O(N·64) iterations, each does a
    // 1-bit left-shift of an M+1-limb accumulator and a conditional
    // subtract of `mod`. Standard textbook long division on bits.
    auto reduce_to_M = [](const U<N>& src, const U<M>& mod, U<M>& dst) {
        // Working accumulator is M+1 limbs (need one extra for the
        // post-shift bit). Represented as r[M] (high) plus r_low (M).
        U<M> r_low; r_low.set_zero();
        uint64_t r_high = 0;
        for (int bit = N * LIMB_BITS - 1; bit >= 0; --bit) {
            // Shift accumulator left by 1.
            uint64_t carry_out = r_low.limbs[M - 1] >> (LIMB_BITS - 1);
            for (int i = M - 1; i > 0; --i)
                r_low.limbs[i] = (r_low.limbs[i] << 1) |
                                 (r_low.limbs[i - 1] >> (LIMB_BITS - 1));
            r_low.limbs[0] = r_low.limbs[0] << 1;
            uint64_t new_high = (r_high << 1) | carry_out;
            // Bring in the next bit of src.
            uint64_t b = (src.limbs[bit / LIMB_BITS] >> (bit % LIMB_BITS)) & 1u;
            r_low.limbs[0] |= b;
            r_high = new_high;
            // Conditional subtract: if (r_high, r_low) >= mod, subtract.
            // r_high is at most 1 (since mod has bit-(M*64-1) set as the
            // top bit, and r before the shift was < mod). Two paths:
            if (r_high != 0 || cmp(r_low, mod) >= 0) {
                U<M> t; sub(t, r_low, mod); r_low = t;
                r_high = 0;
            }
        }
        for (int i = 0; i < M; ++i) dst.limbs[i] = r_low.limbs[i];
    };

    U<M> c_p, c_q, m1, m2;
    reduce_to_M(c, p, c_p);
    reduce_to_M(c, q, c_q);

    modexp_consttime(m1, c_p, dp, p);
    modexp_consttime(m2, c_q, dq, q);

    // h = qInv * (m1 - m2) mod p
    U<M> diff;
    if (cmp(m1, m2) >= 0) {
        sub(diff, m1, m2);
    } else {
        // m1 - m2 is negative; add p once to make positive.
        U<M> t; sub(t, m2, m1);
        sub(diff, p, t);
    }
    // h = qInv * diff mod p
    uint64_t p_inv = compute_n_inv(p);
    U<M> R2_p, diff_mont, qInv_mont, h_mont, h;
    compute_R2_mod_n(R2_p, p, p_inv);
    mont_mul(diff_mont, diff, R2_p, p, p_inv);
    mont_mul(qInv_mont, qInv, R2_p, p, p_inv);
    mont_mul(h_mont, diff_mont, qInv_mont, p, p_inv);
    U<M> one_p; one_p.set_zero(); one_p.limbs[0] = 1;
    mont_mul(h, h_mont, one_p, p, p_inv);

    // out = m2 + h * q (full multi-precision multiply at N limbs).
    U<N> hq;
    {
        U<N> h_ext; h_ext.set_zero();
        for (int i = 0; i < M; ++i) h_ext.limbs[i] = h.limbs[i];
        U<N> q_ext; q_ext.set_zero();
        for (int i = 0; i < M; ++i) q_ext.limbs[i] = q.limbs[i];
        // Schoolbook N-limb multiply truncated to N limbs.
        for (int i = 0; i < N; ++i) hq.limbs[i] = 0;
        for (int i = 0; i < M; ++i) {
            uint64_t carry = 0;
            for (int j = 0; j < M && i + j < N; ++j) {
                u128 prod = (u128)h.limbs[i] * (u128)q.limbs[j]
                            + (u128)hq.limbs[i + j] + (u128)carry;
                hq.limbs[i + j] = static_cast<uint64_t>(prod);
                carry = static_cast<uint64_t>(prod >> 64);
            }
            if (i + M < N) hq.limbs[i + M] += carry;
        }
    }
    U<N> m2_ext; m2_ext.set_zero();
    for (int i = 0; i < M; ++i) m2_ext.limbs[i] = m2.limbs[i];
    add(out, m2_ext, hq);
}

}  // namespace openclicknp::bigint
