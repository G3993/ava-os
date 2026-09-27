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
    std::atomic<float> peak{0.0f};   // post-gain, post-guard: what the engine sees
    // input guard (audio thread): a source that sits at full scale for more
    // than ~250 ms is either clipping the interface or feeding back through
    // us (interface out → mixer → interface in). Each time that happens the
    // guard pulls the input down 6 dB (floor -36 dB); once the input has
    // been quiet for a while it eases back up 1 dB at a time. autoTrim is
    // the factor currently applied (1 = none); clipHold > 0 while the raw
    // input has hit full scale in the last ~1.5 s (UI "CLIP" flag).
    std::atomic<float> autoTrim{1.0f};
    std::atomic<float> clipHold{0.0f};
    float trim_ = 1.0f, trimTarget_ = 1.0f, hotSec_ = 0, coolSec_ = 0;
    StereoRing* ring() { return ring_; }
    void* unit_ = nullptr;
    std::vector<float> scratch_;
private:
    std::atomic<bool> running_{false};
    StereoRing* ring_ = nullptr;
};
