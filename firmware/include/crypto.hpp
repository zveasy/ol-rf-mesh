#pragma once

#include <cstddef>
#include <cstdint>
#include <array>

constexpr std::size_t kAesGcmKeyLen = 32;

struct AesGcmKey {
    std::array<uint8_t, kAesGcmKeyLen> bytes;
};

struct AesGcmResult {
    bool ok;
    std::size_t ciphertext_len;
};

// Encrypts `plaintext` into `ciphertext`, writes tag into `auth_tag`.
AesGcmResult aes_gcm_encrypt(const uint8_t* plaintext,
                             std::size_t plaintext_len,
                             const AesGcmKey& key,
                             const uint8_t* nonce,
                             std::size_t nonce_len,
                             uint8_t* ciphertext,
                             std::size_t max_ciphertext_len,
                             uint8_t* auth_tag,
                             std::size_t auth_tag_len);
