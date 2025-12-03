#include "crypto.hpp"
#include <algorithm>
#include <cstring>

// Stub AES-GCM implementation: copies plaintext to ciphertext, fills auth tag with a simple hash.
AesGcmResult aes_gcm_encrypt(const uint8_t* plaintext,
                             std::size_t plaintext_len,
                             const AesGcmKey& /*key*/,
                             const uint8_t* nonce,
                             std::size_t nonce_len,
                             uint8_t* ciphertext,
                             std::size_t max_ciphertext_len,
                             uint8_t* auth_tag,
                             std::size_t auth_tag_len) {
    if (plaintext_len > max_ciphertext_len) {
        return {false, 0};
    }

    std::memcpy(ciphertext, plaintext, plaintext_len);

    // Lightweight, non-cryptographic tag placeholder.
    uint8_t acc = 0;
    for (std::size_t i = 0; i < plaintext_len; ++i) {
        acc ^= plaintext[i];
        acc = static_cast<uint8_t>((acc << 1) | (acc >> 7));
    }
    for (std::size_t i = 0; i < nonce_len; ++i) {
        acc ^= nonce[i];
    }
    std::fill(auth_tag, auth_tag + auth_tag_len, acc);

    return {true, plaintext_len};
}

AesGcmResult aes_gcm_decrypt(const uint8_t* ciphertext,
                             std::size_t ciphertext_len,
                             const AesGcmKey& /*key*/,
                             const uint8_t* /*nonce*/,
                             std::size_t /*nonce_len*/,
                             const uint8_t* /*auth_tag*/,
                             std::size_t /*auth_tag_len*/,
                             uint8_t* plaintext,
                             std::size_t max_plaintext_len) {
    if (ciphertext_len > max_plaintext_len) {
        return {false, 0};
    }
    std::memcpy(plaintext, ciphertext, ciphertext_len);
    return {true, ciphertext_len};
}
