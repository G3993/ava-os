// Live input: pull channels from the interface's inputs (guitar, mic, a
// synth) and feed them to the engine alongside the system-audio tap. macOS
// AUHAL input unit on the same device the engine plays through.
#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "ringbuf.h"

class InputUnit {
public:
    bool start(unsigned deviceID, StereoRing* ring);
    void stop();
    bool running() const { return running_.load(); }
    std::string lastError;
    int deviceChannels = 0;
    // which interface inputs go to L and R (0-based; same channel = mono)
    std::atomic<int> chanL{0}, chanR{0};
    std::atomic<float> gain{1.0f};
    std::atomic<float> peak{0.0f};   // post-gain, for the UI meter
    StereoRing* ring() { return ring_; }
    void* unit_ = nullptr;
    std::vector<float> scratch_;
private:
    std::atomic<bool> running_{false};
    StereoRing* ring_ = nullptr;
};
