#pragma once

#include "mesh_encode.hpp"
#include <vector>
#include <cstddef>
#include <functional>
#include <queue>
#include <random>

// Simple in-memory radio simulation for host tests.
// Collects transmitted frames and can inject received frames to the stack.
class MockRadio {
public:
    using FrameHandler = std::function<void(const EncryptedFrame&)>;

    void set_receive_handler(FrameHandler handler) { receive_handler_ = handler; }

    void transmit(const EncryptedFrame& frame) { sent_frames_.push_back(frame); }

    void inject(const EncryptedFrame& frame) {
        if (receive_handler_) {
            receive_handler_(frame);
        }
    }

    // Queue-based send with optional drop rate for retry testing.
    void enqueue_to_air(const EncryptedFrame& frame) { air_queue_.push(frame); }

    void pump_air(float drop_prob = 0.0f, unsigned int seed = 123) {
        std::mt19937 rng(seed);
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        while (!air_queue_.empty()) {
            EncryptedFrame f = air_queue_.front();
            air_queue_.pop();
            if (drop_prob > 0.0f && dist(rng) < drop_prob) {
                continue; // simulate drop
            }
            inject(f);
        }
    }

    std::size_t sent_count() const { return sent_frames_.size(); }
    const std::vector<EncryptedFrame>& sent_frames() const { return sent_frames_; }

    void clear() { sent_frames_.clear(); }

private:
    std::vector<EncryptedFrame> sent_frames_;
    FrameHandler receive_handler_;
    std::queue<EncryptedFrame> air_queue_;
};
