// SPDX-License-Identifier: Apache-2.0
//
// C-callable shim that bridges OpenSSL's EVP_CIPHER callbacks to
// openclicknp::aes::aes128_*_block.
//
// EVP_CIPHER_CTX gives us a per-cipher-instance impl_ctx_size buffer
// where we stash the expanded round keys. We treat that buffer as a
// `Key128` (which is just a 176-byte plain-old-data array — see
// runtime/include/openclicknp/aes128.hpp).
#include "openclicknp/aes128.hpp"

#include <cstring>

extern "C" int openclicknp_aes128_expand(void* opaque, const unsigned char* key) {
    auto* k = static_cast<openclicknp::aes::Key128*>(opaque);
    openclicknp::aes::aes128_key_expand(*k, key);
    return 1;
}

extern "C" int openclicknp_aes128_encrypt_block(unsigned char* out,
                                                const unsigned char* in,
                                                void* opaque) {
    const auto* k = static_cast<const openclicknp::aes::Key128*>(opaque);
    openclicknp::aes::aes128_encrypt_block(out, in, *k);
    return 1;
}

extern "C" int openclicknp_aes128_decrypt_block(unsigned char* out,
                                                const unsigned char* in,
                                                void* opaque) {
    const auto* k = static_cast<const openclicknp::aes::Key128*>(opaque);
    openclicknp::aes::aes128_decrypt_block(out, in, *k);
    return 1;
}
