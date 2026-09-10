// System-audio capture via CoreAudio process tap (macOS 14.4+).
// Taps the global stereo mix of everything playing (excluding this app,
// to avoid feeding our own output back in) into a lock-free ring.
#pragma once
#include <atomic>
#include <string>
#include "ringbuf.h"

class SystemTap {
public:
    bool start(StereoRing* ring);
    void stop();
    bool running() const { return running_.load(); }
    std::string lastError;
    std::atomic<float> inputPeak{0};
    double tapSampleRate = 48000.0;
    StereoRing* ring() { return ring_; }

private:
    std::atomic<bool> running_{false};
    unsigned tapID_ = 0, aggID_ = 0;
    void* procID_ = nullptr;
    StereoRing* ring_ = nullptr;
    friend struct TapTrampoline;
};
