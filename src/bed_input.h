// Live bed input: a DAW plays AVA directly, over a multichannel device
// (BlackHole 16ch, or the interface's own loopback), while the piece is
// being written — no export loop. Seven consecutive device channels:
//   1-2 music (goes to the music pair)  ·  3 HEAD · 4 HEART · 5 BELLY · 6 BUTT · 7 FEET
// the same order as a stem set. Any zone switched back to "engine" is filled
// by the live engine from the music, so a composer can author only the
// layers they care about. macOS AUHAL capture (audio_in.mm).
#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "ringbuf.h"
#include "engine.h"

class BedInput {
public:
    static constexpr int kCh = 7;
    bool start(unsigned deviceID);       // begins capturing; false + lastError
    void stop();
    bool running() const { return running_.load(); }
    std::string lastError;
    unsigned deviceID = 0;
    int deviceChannels = 0;
    // 0-based device channel the 7-wide block starts at (0 = channels 1-7)
    std::atomic<int> firstChan{0};
    // per zone: 1 = this zone comes from the DAW, 0 = the engine fills it
    std::atomic<int> zoneFromDaw[NZONES]{{1}, {1}, {1}, {1}, {1}};
    std::atomic<float> peak[kCh];        // UI meters (audio thread, decayed)
    MultiRing ring;
    // capture side (audio thread)
    std::vector<float> scratch_;
    void* unit_ = nullptr;
    void setRunning(bool b) { running_.store(b); }
private:
    std::atomic<bool> running_{false};
};
