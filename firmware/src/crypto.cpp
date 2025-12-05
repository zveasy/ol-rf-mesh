#include "crypto.hpp"
#include <algorithm>
#include <cstring>

namespace {
// Minimal SipHash-2-4 for MAC; not constant time, but sufficient for host tests.
inline uint64_t rotl64(uint64_t x, int b) {
    return (x << b) | (x >> (64 - b));
}

uint64_t siphash24(const uint8_t* data, std::size_t len, const uint8_t key[16]) {
    uint64_t k0 = 0, k1 = 0;
    std::memcpy(&k0, key, 8);
    std::memcpy(&k1, key + 8, 8);

    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;

    const uint8_t* ptr = data;
    const uint8_t* end = data + len - (len % 8);

    while (ptr != end) {
        uint64_t m = 0;
        std::memcpy(&m, ptr, 8);
        v3 ^= m;
        for (int i = 0; i < 2; ++i) {
            v0 += v1; v1 = rotl64(v1, 13); v1 ^= v0; v0 = rotl64(v0, 32);
            v2 += v3; v3 = rotl64(v3, 16); v3 ^= v2;
            v0 += v3; v3 = rotl64(v3, 21); v3 ^= v0;
            v2 += v1; v1 = rotl64(v1, 17); v1 ^= v2; v2 = rotl64(v2, 32);
        }
        v0 ^= m;
        ptr += 8;
    }

    uint64_t last = static_cast<uint64_t>(len) << 56;
    switch (len % 8) {
        case 7: last |= static_cast<uint64_t>(ptr[6]) << 48; [[fallthrough]];
        case 6: last |= static_cast<uint64_t>(ptr[5]) << 40; [[fallthrough]];
        case 5: last |= static_cast<uint64_t>(ptr[4]) << 32; [[fallthrough]];
        case 4: last |= static_cast<uint64_t>(ptr[3]) << 24; [[fallthrough]];
        case 3: last |= static_cast<uint64_t>(ptr[2]) << 16; [[fallthrough]];
        case 2: last |= static_cast<uint64_t>(ptr[1]) << 8; [[fallthrough]];
        case 1: last |= static_cast<uint64_t>(ptr[0]); break;
        default: break;
    }

    v3 ^= last;
    for (int i = 0; i < 2; ++i) {
        v0 += v1; v1 = rotl64(v1, 13); v1 ^= v0; v0 = rotl64(v0, 32);
        v2 += v3; v3 = rotl64(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl64(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl64(v1, 17); v1 ^= v2; v2 = rotl64(v2, 32);
    }
    v0 ^= last;

    v2 ^= 0xff;
    for (int i = 0; i < 4; ++i) {
        v0 += v1; v1 = rotl64(v1, 13); v1 ^= v0; v0 = rotl64(v0, 32);
        v2 += v3; v3 = rotl64(v3, 16); v3 ^= v2;
        v0 += v3; v3 = rotl64(v3, 21); v3 ^= v0;
        v2 += v1; v1 = rotl64(v1, 17); v1 ^= v2; v2 = rotl64(v2, 32);
    }

    return v0 ^ v1 ^ v2 ^ v3;
}

// Derive a simple keystream from key + nonce using xorshift; deterministic for test builds.
uint64_t keystream_seed(const AesGcmKey& key, const uint8_t* nonce, std::size_t nonce_len) {
    uint8_t mac_key[16]{0};
    std::memcpy(mac_key, key.bytes.data(), std::min<std::size_t>(key.bytes.size(), sizeof(mac_key)));
    return siphash24(nonce, nonce_len, mac_key);
}

void xor_keystream(uint64_t seed, const uint8_t* in, std::size_t len, uint8_t* out) {
    uint64_t state = seed;
    for (std::size_t i = 0; i < len; ++i) {
        // xorshift* variant for lightweight keystream
        state ^= state >> 12;
        state ^= state << 25;
        state ^= state >> 27;
        state *= 0x2545F4914F6CDD1DULL;
        uint8_t ks_byte = static_cast<uint8_t>(state >> 56);
        out[i] = in[i] ^ ks_byte;
    }
}

bool constant_time_eq(const uint8_t* a, const uint8_t* b, std::size_t len) {
    uint8_t acc = 0;
    for (std::size_t i = 0; i < len; ++i) {
        acc |= static_cast<uint8_t>(a[i] ^ b[i]);
    }
    return acc == 0;
}
} // namespace

AesGcmResult aes_gcm_encrypt(const uint8_t* plaintext,
                             std::size_t plaintext_len,
                             const AesGcmKey& key,
                             const uint8_t* nonce,
                             std::size_t nonce_len,
                             uint8_t* ciphertext,
                             std::size_t max_ciphertext_len,
                             uint8_t* auth_tag,
                             std::size_t auth_tag_len) {
    if (plaintext_len > max_ciphertext_len || auth_tag_len == 0 || nonce_len == 0) {
        return {false, 0};
    }

    const uint64_t seed = keystream_seed(key, nonce, nonce_len);
    xor_keystream(seed, plaintext, plaintext_len, ciphertext);

    // MAC over nonce || ciphertext to detect tampering/replay.
    uint8_t mac_key[16]{0};
    std::memcpy(mac_key, key.bytes.data(), std::min<std::size_t>(key.bytes.size(), sizeof(mac_key)));
    uint64_t mac = siphash24(nonce, nonce_len, mac_key);
    mac ^= siphash24(ciphertext, plaintext_len, mac_key);

    const std::size_t emit_len = std::min<std::size_t>(auth_tag_len, sizeof(mac));
    std::memcpy(auth_tag, &mac, emit_len);
    if (emit_len < auth_tag_len) {
        std::fill(auth_tag + emit_len, auth_tag + auth_tag_len, 0);
    }

    return {true, plaintext_len};
}

AesGcmResult aes_gcm_decrypt(const uint8_t* ciphertext,
                             std::size_t ciphertext_len,
                             const AesGcmKey& key,
                             const uint8_t* nonce,
                             std::size_t nonce_len,
                             const uint8_t* auth_tag,
                             std::size_t auth_tag_len,
                             uint8_t* plaintext,
                             std::size_t max_plaintext_len) {
    if (ciphertext_len > max_plaintext_len || auth_tag_len == 0 || nonce_len == 0) {
        return {false, 0};
    }

    uint8_t mac_key[16]{0};
    std::memcpy(mac_key, key.bytes.data(), std::min<std::size_t>(key.bytes.size(), sizeof(mac_key)));
    uint64_t mac_calc = siphash24(nonce, nonce_len, mac_key);
    mac_calc ^= siphash24(ciphertext, ciphertext_len, mac_key);

    uint64_t mac_in = 0;
    std::memcpy(&mac_in, auth_tag, std::min<std::size_t>(sizeof(mac_in), auth_tag_len));
    uint8_t mac_in_buf[sizeof(mac_calc)]{};
    std::memcpy(mac_in_buf, &mac_in, sizeof(mac_in));
    uint8_t mac_calc_buf[sizeof(mac_calc)]{};
    std::memcpy(mac_calc_buf, &mac_calc, sizeof(mac_calc));

    if (!constant_time_eq(mac_in_buf, mac_calc_buf, sizeof(mac_calc_buf))) {
        return {false, 0};
    }

    const uint64_t seed = keystream_seed(key, nonce, nonce_len);
    xor_keystream(seed, ciphertext, ciphertext_len, plaintext);
    return {true, ciphertext_len};
}
