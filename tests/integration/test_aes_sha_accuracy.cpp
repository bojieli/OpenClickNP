// SPDX-License-Identifier: Apache-2.0
//
// NIST-vector accuracy tests for the symmetric crypto primitives.
//
// What this exercises
//   AES-128 (FIPS 197 Appendix C.1) — encrypt + decrypt round trip.
//   AES-128 (NIST AESAVS GFSbox / VarTxt subset) — additional
//     encrypt-only vectors. We also cross-check 200 random plaintexts
//     against OpenSSL's EVP_aes_128_ecb.
//   AES-128-CTR (NIST SP 800-38A F.5.1) — 4-block keystream + 200
//     random plaintexts cross-checked against OpenSSL's EVP_aes_128_ctr.
//   SHA-1 (FIPS 180-4 §A) — 'abc', 448-bit Appendix-A msg, empty,
//     million-a; plus 200 random messages of varying length cross-
//     checked against OpenSSL's EVP_sha1.
#include "openclicknp/aes128.hpp"
#include "openclicknp/sha1.hpp"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

using namespace openclicknp;

static int parse_hex(uint8_t* out, const char* hex, int len) {
    for (int i = 0; i < len; ++i) {
        unsigned int v;
        if (std::sscanf(hex + 2 * i, "%02x", &v) != 1) return 0;
        out[i] = (uint8_t)v;
    }
    return 1;
}

static int hex_eq(const uint8_t* got, const char* want_hex, int len) {
    for (int i = 0; i < len; ++i) {
        unsigned int v;
        if (std::sscanf(want_hex + 2 * i, "%02x", &v) != 1) return 0;
        if (got[i] != (uint8_t)v) return 0;
    }
    return 1;
}

int fails = 0;

// ----- AES-128 -----

static void aes_kat_enc(const char* tag, const char* key_hex,
                        const char* pt_hex, const char* ct_hex) {
    uint8_t key[16], pt[16], ct[16], got[16], back[16];
    parse_hex(key, key_hex, 16);
    parse_hex(pt,  pt_hex,  16);
    parse_hex(ct,  ct_hex,  16);
    aes::Key128 k; aes::aes128_key_expand(k, key);
    aes::aes128_encrypt_block(got, pt, k);
    if (std::memcmp(got, ct, 16) != 0) {
        std::printf("  AES enc %-18s FAIL\n", tag); ++fails;
    } else {
        std::printf("  AES enc %-18s ✓\n", tag);
    }
    aes::aes128_decrypt_block(back, got, k);
    if (std::memcmp(back, pt, 16) != 0) {
        std::printf("  AES dec %-18s FAIL\n", tag); ++fails;
    } else {
        std::printf("  AES dec %-18s ✓\n", tag);
    }
}

static void aes_ecb_random_vs_openssl(int n) {
    std::printf("AES-128-ECB %d random plaintexts vs OpenSSL:\n", n);
    std::mt19937_64 rng(0xAA55AA55AA55AA55ULL);
    int local_fail = 0;
    for (int it = 0; it < n; ++it) {
        uint8_t key[16], pt[16], got[16], want[16];
        for (int i = 0; i < 16; ++i) { key[i] = (uint8_t)rng(); pt[i] = (uint8_t)rng(); }
        aes::Key128 k; aes::aes128_key_expand(k, key);
        aes::aes128_encrypt_block(got, pt, k);
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key, NULL);
        EVP_CIPHER_CTX_set_padding(ctx, 0);
        int outl = 0;
        EVP_EncryptUpdate(ctx, want, &outl, pt, 16);
        EVP_CIPHER_CTX_free(ctx);
        if (std::memcmp(got, want, 16) != 0) {
            if (local_fail < 3) std::printf("  iter %d MISMATCH\n", it);
            ++local_fail;
        }
    }
    if (local_fail == 0) std::printf("  all %d match OpenSSL ✓\n", n);
    else { std::printf("  %d mismatches\n", local_fail); fails += local_fail; }
}

static void aes_ctr_random_vs_openssl(int n) {
    std::printf("AES-128-CTR %d random msgs vs OpenSSL:\n", n);
    std::mt19937_64 rng(0xBADBADBADBADBADBULL);
    int local_fail = 0;
    for (int it = 0; it < n; ++it) {
        uint8_t key[16], iv[16];
        for (int i = 0; i < 16; ++i) { key[i] = (uint8_t)rng(); iv[i] = (uint8_t)rng(); }
        // Length 1..127 bytes (forces partial-last-block handling).
        size_t len = 1 + (size_t)(rng() % 127);
        std::vector<uint8_t> pt(len), got(len), want(len);
        for (size_t i = 0; i < len; ++i) pt[i] = (uint8_t)rng();
        aes::Key128 k; aes::aes128_key_expand(k, key);
        aes::aes128_ctr_xcrypt(got.data(), pt.data(), len, k, iv);
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        EVP_EncryptInit_ex(ctx, EVP_aes_128_ctr(), NULL, key, iv);
        int outl = 0;
        EVP_EncryptUpdate(ctx, want.data(), &outl, pt.data(), (int)len);
        EVP_CIPHER_CTX_free(ctx);
        if (std::memcmp(got.data(), want.data(), len) != 0) {
            if (local_fail < 3) std::printf("  iter %d (len=%zu) MISMATCH\n", it, len);
            ++local_fail;
        }
    }
    if (local_fail == 0) std::printf("  all %d match OpenSSL ✓\n", n);
    else { std::printf("  %d mismatches\n", local_fail); fails += local_fail; }
}

// ----- SHA-1 -----

static void sha1_kat(const char* label, const uint8_t* msg, size_t len,
                     const char* want_hex) {
    uint8_t got[20];
    sha1::hash(got, msg, len);
    if (!hex_eq(got, want_hex, 20)) {
        std::printf("  SHA-1 %-12s FAIL\n", label); ++fails;
    } else {
        std::printf("  SHA-1 %-12s ✓\n", label);
    }
}

static void sha1_random_vs_openssl(int n) {
    std::printf("SHA-1 %d random msgs vs OpenSSL:\n", n);
    std::mt19937_64 rng(0xC0DEFACEC0DEFACEULL);
    int local_fail = 0;
    for (int it = 0; it < n; ++it) {
        // Length 0..1023 bytes.
        size_t len = (size_t)(rng() % 1024);
        std::vector<uint8_t> msg(len);
        for (size_t i = 0; i < len; ++i) msg[i] = (uint8_t)rng();
        uint8_t got[20], want[20];
        sha1::hash(got, msg.data(), len);
        unsigned int wlen = 0;
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha1(), NULL);
        EVP_DigestUpdate(ctx, msg.data(), len);
        EVP_DigestFinal_ex(ctx, want, &wlen);
        EVP_MD_CTX_free(ctx);
        if (std::memcmp(got, want, 20) != 0) {
            if (local_fail < 3) std::printf("  iter %d (len=%zu) MISMATCH\n", it, len);
            ++local_fail;
        }
    }
    if (local_fail == 0) std::printf("  all %d match OpenSSL ✓\n", n);
    else { std::printf("  %d mismatches\n", local_fail); fails += local_fail; }
}

int main() {
    std::printf("FIPS 197 Appendix C.1 (AES-128):\n");
    aes_kat_enc("appendix-c1",
        "000102030405060708090a0b0c0d0e0f",
        "00112233445566778899aabbccddeeff",
        "69c4e0d86a7b0430d8cdb78070b4c55a");

    std::printf("NIST AESAVS GFSbox/VarTxt subset:\n");
    aes_kat_enc("gfs-zero",
        "00000000000000000000000000000000",
        "00000000000000000000000000000000",
        "66e94bd4ef8a2c3b884cfa59ca342b2e");
    aes_kat_enc("vt-allff",
        "00000000000000000000000000000000",
        "ffffffffffffffffffffffffffffffff",
        "3f5b8cc9ea855a0afa7347d23e8d664e");

    std::printf("SP 800-38A F.5.1 (AES-128-CTR):\n");
    {
        uint8_t key[16], iv[16], pt[64], expected[64], got[64];
        parse_hex(key, "2b7e151628aed2a6abf7158809cf4f3c", 16);
        parse_hex(iv,  "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff", 16);
        parse_hex(pt, "6bc1bee22e409f96e93d7e117393172a"
                      "ae2d8a571e03ac9c9eb76fac45af8e51"
                      "30c81c46a35ce411e5fbc1191a0a52ef"
                      "f69f2445df4f9b17ad2b417be66c3710", 64);
        parse_hex(expected,
                      "874d6191b620e3261bef6864990db6ce"
                      "9806f66b7970fdff8617187bb9fffdff"
                      "5ae4df3edbd5d35e5b4f09020db03eab"
                      "1e031dda2fbe03d1792170a0f3009cee", 64);
        aes::Key128 k; aes::aes128_key_expand(k, key);
        aes::aes128_ctr_xcrypt(got, pt, 64, k, iv);
        if (std::memcmp(got, expected, 64) != 0) {
            std::printf("  CTR 4-block FAIL\n"); ++fails;
        } else {
            std::printf("  CTR 4-block ✓\n");
        }
    }

    std::printf("FIPS 180-4 (SHA-1):\n");
    sha1_kat("'abc'",     (const uint8_t*)"abc", 3,
        "a9993e364706816aba3e25717850c26c9cd0d89d");
    sha1_kat("448-bit",   (const uint8_t*)"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    sha1_kat("empty",     (const uint8_t*)"", 0,
        "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    {
        std::vector<uint8_t> million(1000000, 'a');
        sha1_kat("million-a", million.data(), 1000000,
            "34aa973cd4c4daa4f61eeb2bdbad27316534016f");
    }

    aes_ecb_random_vs_openssl(200);
    aes_ctr_random_vs_openssl(200);
    sha1_random_vs_openssl(200);

    if (fails) { std::printf("\nFAIL: %d total\n", fails); return 1; }
    std::printf("\nALL AES + SHA NIST/FIPS + OpenSSL CROSS-CHECKS PASS\n");
    return 0;
}
