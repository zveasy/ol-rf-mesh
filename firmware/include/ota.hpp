#pragma once

#include <cstdint>
#include <cstddef>

enum class OtaState : uint8_t {
    Idle = 0,
    Downloading,
    Verifying,
    Applying,
    Rollback,
    Failed,
};

struct OtaChunk {
    uint32_t offset;
    const uint8_t* data;
    std::size_t len;
};

struct OtaStatus {
    OtaState state;
    uint32_t current_offset;
    uint32_t total_size;
    bool signature_valid;
};

void init_ota();
void ota_apply_chunk(const OtaChunk& chunk);
bool ota_verify_and_mark();
OtaStatus ota_status();
