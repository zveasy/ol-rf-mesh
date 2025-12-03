#pragma once

#include "telemetry.hpp"
#include <array>
#include <cstddef>
#include "crypto.hpp"

constexpr std::size_t kMaxMeshFrameLen = 256;
constexpr std::size_t kMaxCipherLen = kMaxMeshFrameLen + 32;

struct EncodedFrame {
    std::array<uint8_t, kMaxMeshFrameLen> bytes;
    std::size_t len;
};

struct EncryptedFrame {
    std::array<uint8_t, kMaxCipherLen> bytes;
    std::size_t len;
};

EncodedFrame encode_mesh_frame(const MeshFrame& frame);
EncryptedFrame encrypt_mesh_frame(const MeshFrame& frame, const AesGcmKey& key);
bool decode_mesh_frame(const EncryptedFrame& enc, const AesGcmKey& key, MeshFrame& out);
