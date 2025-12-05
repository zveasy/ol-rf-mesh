#include "ota.hpp"
#include <cstdio>

namespace {
OtaStatus g_status{
    OtaState::Idle,
    0,
    0,
    false
};
} // namespace

void init_ota() {
    g_status = {OtaState::Idle, 0, 0, false};
}

void ota_apply_chunk(const OtaChunk& chunk) {
    if (g_status.state == OtaState::Idle) {
        g_status.state = OtaState::Downloading;
    }
    g_status.current_offset = chunk.offset + static_cast<uint32_t>(chunk.len);
    g_status.total_size = g_status.total_size == 0 ? g_status.current_offset : g_status.total_size;
    std::printf("[OTA] Received chunk len=%zu offset=%zu\n", chunk.len, static_cast<std::size_t>(chunk.offset));
}

bool ota_verify_and_mark() {
    g_status.state = OtaState::Verifying;
    g_status.signature_valid = true; // stub: assume OK
    g_status.state = g_status.signature_valid ? OtaState::Applying : OtaState::Failed;
    return g_status.signature_valid;
}

OtaStatus ota_status() {
    return g_status;
}
