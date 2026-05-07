// SPDX-License-Identifier: Apache-2.0
//
// AES-128 — full FIPS 197 implementation in plain C++17.
//
// What this provides
//   • aes128_key_expand:   key schedule (44 32-bit round-key words)
//   • aes128_encrypt_block: 16-byte block encrypt (ECB primitive)
//   • aes128_decrypt_block: 16-byte block decrypt (ECB primitive)
//   • aes128_ctr_xcrypt:    CTR mode (encrypt = decrypt; XORs with
//                          AES(counter) keystream).
//
// All operations are deterministic, no STL allocators in the hot path,
// constant-trip-count loops, no recursion → HLS-synthesis friendly.
//
// References (no code copied — implementation written from spec):
//   FIPS 197, "Advanced Encryption Standard", Nov 2001
//   SP 800-38A, "Block Cipher Modes of Operation", Dec 2001 (CTR mode)
#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace openclicknp::aes {

// FIPS 197 §5.1.1 (Table 4) — forward S-box.
inline constexpr std::array<uint8_t, 256> sbox = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

// FIPS 197 §5.3.2 (Table 6) — inverse S-box.
inline constexpr std::array<uint8_t, 256> inv_sbox = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

// Multiplication by 2 in GF(2^8) with reduction polynomial x^8+x^4+x^3+x+1.
inline constexpr uint8_t xtime(uint8_t x) {
    return static_cast<uint8_t>((x << 1) ^ ((x >> 7) ? 0x1b : 0x00));
}

// FIPS 197 Round constants for AES-128 (rcon[1..10] = 0x01,0x02,...,0x36).
inline constexpr std::array<uint8_t, 11> rcon = {
    0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};

// 11 round keys × 16 bytes = 176 bytes total.
struct Key128 {
    std::array<uint8_t, 176> rk{};
};

// Compute the round-key schedule from a 16-byte cipher key (FIPS 197 §5.2).
inline void aes128_key_expand(Key128& out, const uint8_t* key) {
    for (int i = 0; i < 16; ++i) out.rk[i] = key[i];
    for (int i = 16; i < 176; i += 4) {
        uint8_t t0 = out.rk[i - 4];
        uint8_t t1 = out.rk[i - 3];
        uint8_t t2 = out.rk[i - 2];
        uint8_t t3 = out.rk[i - 1];
        if ((i % 16) == 0) {
            // RotWord
            uint8_t s0 = t1, s1 = t2, s2 = t3, s3 = t0;
            // SubWord
            t0 = sbox[s0]; t1 = sbox[s1]; t2 = sbox[s2]; t3 = sbox[s3];
            // XOR Rcon
            t0 ^= rcon[i / 16];
        }
        out.rk[i + 0] = static_cast<uint8_t>(out.rk[i - 16 + 0] ^ t0);
        out.rk[i + 1] = static_cast<uint8_t>(out.rk[i - 16 + 1] ^ t1);
        out.rk[i + 2] = static_cast<uint8_t>(out.rk[i - 16 + 2] ^ t2);
        out.rk[i + 3] = static_cast<uint8_t>(out.rk[i - 16 + 3] ^ t3);
    }
}

// Forward AES-128 block encryption (FIPS 197 §5.1).
inline void aes128_encrypt_block(uint8_t* out, const uint8_t* in,
                                 const Key128& k) {
    uint8_t s[16];
    for (int i = 0; i < 16; ++i) s[i] = in[i] ^ k.rk[i];

    for (int round = 1; round < 10; ++round) {
        uint8_t t[16];
        // SubBytes
        for (int i = 0; i < 16; ++i) t[i] = sbox[s[i]];
        // ShiftRows: row r is rotated left by r positions in column-major
        // state (c, r) at index 4*c + r.
        s[ 0] = t[ 0]; s[ 4] = t[ 4]; s[ 8] = t[ 8]; s[12] = t[12];
        s[ 1] = t[ 5]; s[ 5] = t[ 9]; s[ 9] = t[13]; s[13] = t[ 1];
        s[ 2] = t[10]; s[ 6] = t[14]; s[10] = t[ 2]; s[14] = t[ 6];
        s[ 3] = t[15]; s[ 7] = t[ 3]; s[11] = t[ 7]; s[15] = t[11];
        // MixColumns
        for (int c = 0; c < 4; ++c) {
            int b = c * 4;
            uint8_t a0 = s[b+0], a1 = s[b+1], a2 = s[b+2], a3 = s[b+3];
            uint8_t x0 = a0 ^ a1 ^ a2 ^ a3;
            uint8_t y0 = static_cast<uint8_t>(a0 ^ x0 ^ xtime((uint8_t)(a0 ^ a1)));
            uint8_t y1 = static_cast<uint8_t>(a1 ^ x0 ^ xtime((uint8_t)(a1 ^ a2)));
            uint8_t y2 = static_cast<uint8_t>(a2 ^ x0 ^ xtime((uint8_t)(a2 ^ a3)));
            uint8_t y3 = static_cast<uint8_t>(a3 ^ x0 ^ xtime((uint8_t)(a3 ^ a0)));
            s[b+0] = y0; s[b+1] = y1; s[b+2] = y2; s[b+3] = y3;
        }
        // AddRoundKey
        for (int i = 0; i < 16; ++i) s[i] ^= k.rk[round * 16 + i];
    }

    // Final round: SubBytes + ShiftRows + AddRoundKey (no MixColumns)
    {
        uint8_t t[16];
        for (int i = 0; i < 16; ++i) t[i] = sbox[s[i]];
        s[ 0] = t[ 0]; s[ 4] = t[ 4]; s[ 8] = t[ 8]; s[12] = t[12];
        s[ 1] = t[ 5]; s[ 5] = t[ 9]; s[ 9] = t[13]; s[13] = t[ 1];
        s[ 2] = t[10]; s[ 6] = t[14]; s[10] = t[ 2]; s[14] = t[ 6];
        s[ 3] = t[15]; s[ 7] = t[ 3]; s[11] = t[ 7]; s[15] = t[11];
        for (int i = 0; i < 16; ++i) s[i] ^= k.rk[10 * 16 + i];
    }
    for (int i = 0; i < 16; ++i) out[i] = s[i];
}

// GF(2^8) multiplication helpers used by InvMixColumns.
inline constexpr uint8_t gmul2(uint8_t x)  { return xtime(x); }
inline constexpr uint8_t gmul4(uint8_t x)  { return xtime(xtime(x)); }
inline constexpr uint8_t gmul8(uint8_t x)  { return xtime(xtime(xtime(x))); }
inline constexpr uint8_t gmul9(uint8_t x)  { return static_cast<uint8_t>(gmul8(x) ^ x); }
inline constexpr uint8_t gmul11(uint8_t x) { return static_cast<uint8_t>(gmul8(x) ^ gmul2(x) ^ x); }
inline constexpr uint8_t gmul13(uint8_t x) { return static_cast<uint8_t>(gmul8(x) ^ gmul4(x) ^ x); }
inline constexpr uint8_t gmul14(uint8_t x) { return static_cast<uint8_t>(gmul8(x) ^ gmul4(x) ^ gmul2(x)); }

// AES-128 block decryption (FIPS 197 §5.3).
//
// The algorithm is the equivalent inverse cipher: at each round we
// undo MixColumns, ShiftRows, SubBytes, and AddRoundKey in the order
// that exactly inverts encryption. We follow the §5.3 "straightforward"
// inverse (not the §5.3.5 equivalent inverse with reordered ops).
inline void aes128_decrypt_block(uint8_t* out, const uint8_t* in,
                                 const Key128& k) {
    uint8_t s[16];
    // First AddRoundKey using last round key.
    for (int i = 0; i < 16; ++i) s[i] = in[i] ^ k.rk[10 * 16 + i];

    for (int round = 9; round >= 1; --round) {
        uint8_t t[16];
        // InvShiftRows: row r is rotated RIGHT by r positions.
        // i.e., move s[c*4+r] to t[((c-r) mod 4)*4 + r] = t[((c+4-r)%4)*4 + r].
        // Row 0 unchanged; row 1 shifted right by 1; row 2 by 2; row 3 by 3.
        t[ 0] = s[ 0]; t[ 4] = s[ 4]; t[ 8] = s[ 8]; t[12] = s[12];           // row 0
        t[ 1] = s[13]; t[ 5] = s[ 1]; t[ 9] = s[ 5]; t[13] = s[ 9];           // row 1
        t[ 2] = s[10]; t[ 6] = s[14]; t[10] = s[ 2]; t[14] = s[ 6];           // row 2
        t[ 3] = s[ 7]; t[ 7] = s[11]; t[11] = s[15]; t[15] = s[ 3];           // row 3
        // InvSubBytes
        for (int i = 0; i < 16; ++i) s[i] = inv_sbox[t[i]];
        // AddRoundKey for this round.
        for (int i = 0; i < 16; ++i) s[i] ^= k.rk[round * 16 + i];
        // InvMixColumns: each column transforms by the matrix
        //   [0e 0b 0d 09]
        //   [09 0e 0b 0d]
        //   [0d 09 0e 0b]
        //   [0b 0d 09 0e]
        for (int c = 0; c < 4; ++c) {
            int b = c * 4;
            uint8_t a0 = s[b+0], a1 = s[b+1], a2 = s[b+2], a3 = s[b+3];
            s[b+0] = static_cast<uint8_t>(gmul14(a0) ^ gmul11(a1) ^ gmul13(a2) ^ gmul9 (a3));
            s[b+1] = static_cast<uint8_t>(gmul9 (a0) ^ gmul14(a1) ^ gmul11(a2) ^ gmul13(a3));
            s[b+2] = static_cast<uint8_t>(gmul13(a0) ^ gmul9 (a1) ^ gmul14(a2) ^ gmul11(a3));
            s[b+3] = static_cast<uint8_t>(gmul11(a0) ^ gmul13(a1) ^ gmul9 (a2) ^ gmul14(a3));
        }
    }
    // Final inverse round: InvShiftRows + InvSubBytes + AddRoundKey, no InvMixColumns.
    {
        uint8_t t[16];
        t[ 0] = s[ 0]; t[ 4] = s[ 4]; t[ 8] = s[ 8]; t[12] = s[12];
        t[ 1] = s[13]; t[ 5] = s[ 1]; t[ 9] = s[ 5]; t[13] = s[ 9];
        t[ 2] = s[10]; t[ 6] = s[14]; t[10] = s[ 2]; t[14] = s[ 6];
        t[ 3] = s[ 7]; t[ 7] = s[11]; t[11] = s[15]; t[15] = s[ 3];
        for (int i = 0; i < 16; ++i) s[i] = inv_sbox[t[i]];
        for (int i = 0; i < 16; ++i) s[i] ^= k.rk[i];
    }
    for (int i = 0; i < 16; ++i) out[i] = s[i];
}

// CTR mode (SP 800-38A §6.5).
//
// `iv` is the initial 16-byte counter block; we increment the *low 64
// bits* per block (the standard Linux/OpenSSL convention; full-block
// big-endian increment is also valid). `len` may be any byte count
// (not just a multiple of 16) — partial last block is XORed against
// the truncated keystream.
inline void aes128_ctr_xcrypt(uint8_t* out, const uint8_t* in, std::size_t len,
                              const Key128& k, const uint8_t iv[16]) {
    uint8_t ctr[16];
    for (int i = 0; i < 16; ++i) ctr[i] = iv[i];

    std::size_t off = 0;
    while (off < len) {
        uint8_t ks[16];
        aes128_encrypt_block(ks, ctr, k);
        std::size_t chunk = (len - off) < 16 ? (len - off) : 16;
        for (std::size_t i = 0; i < chunk; ++i)
            out[off + i] = in[off + i] ^ ks[i];
        // Increment counter (low 64 bits, big-endian).
        for (int i = 15; i >= 8; --i) {
            ctr[i] = static_cast<uint8_t>(ctr[i] + 1);
            if (ctr[i] != 0) break;
        }
        off += chunk;
    }
}

}  // namespace openclicknp::aes
