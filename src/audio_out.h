// Output stage: enumerate CoreAudio output devices (your interface),
// run an AUHAL unit on the chosen device, pull tapped stereo from the ring,
// run the engine, and lay out channels:
//   ch1 L · ch2 R (passthrough of what you're playing)
//   ch3 HEAD · ch4 HEART · ch5 BELLY · ch6 ROOT · ch7 FEET
// Devices with fewer channels get a truncated map (stereo-only devices can
// optionally monitor the vibration bus mixed into L/R).
#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "ringbuf.h"
#include "engine.h"

struct OutDevice {
    unsigned id;
    std::string name;
    int channels;
    double sampleRate;
};

std::vector<OutDevice> listOutputDevices();

// hardware output volume scalars on a device (the knob between us and the
// DACs): average across settable elements, or -1 if the device exposes none
float deviceHwVolume(unsigned deviceID);
// push every settable output volume scalar to 1.0 and unmute all elements;
// returns true if the device exposed anything settable
bool maxDeviceHwVolume(unsigned deviceID);

class OutputUnit {
public:
    bool start(unsigned deviceID, Engine* engine, StereoRing* ring);
    void stop();
    bool running() const { return running_.load(); }
    std::string lastError;
    int activeChannels = 0;
    // per-device-channel output peak, post master/trim — what actually leaves
    // the interface toward each amp. Written on the audio thread with a slow
    // per-block decay; the health monitor reads them.
    static constexpr int kMaxCh = 16;
    std::atomic<float> chanPeak[kMaxCh];
    // channel finder: ping(c) plays a 0.9 s 55 Hz burst on device channel c
    // alone (bypasses zone routing) so an unknown amp/transducer can be
    // located by feel. <0 = idle. Cleared by the audio thread when done.
    std::atomic<int> pingChan{-1};
    void ping(int c) { pingLeft_.store(43200); pingChan.store(c); }
    // optional second stereo ring (live input from the interface); summed
    // with the tap before the engine. Set by the app, may be null.
    StereoRing* inRing = nullptr;
    // live input goes to the ENGINE only by default (felt, not heard); on =
    // also mixed into the music / surround sends
    std::atomic<bool> inToSpeakers{false};
    std::atomic<int> pingLeft_{0}; // samples remaining (audio thread owned)
    float pingPhase_ = 0;
    // thermal guard state per device channel (zone channels only): running
    // mean-square (~45 s) and the gain currently applied. chanHeat = fraction
    // of the RMS budget (1.0 = at the limit) for the health monitor.
    std::atomic<float> chanHeat[kMaxCh];
    std::atomic<float> chanThermGain[kMaxCh];
    float heatMs_[kMaxCh] = {0};
    float thermG_[kMaxCh] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

private:
    std::atomic<bool> running_{false};
    void* unit_ = nullptr; // AudioUnit
    Engine* engine_ = nullptr;
    StereoRing* ring_ = nullptr;
    friend struct OutTrampoline;
public:
    Engine* engine() { return engine_; }
    StereoRing* ring() { return ring_; }
};

// shared mixer (audio_mix.cpp) used by every platform backend
void mixOutputBlock(OutputUnit* self, Engine* eng, const float* L, const float* R,
                    float* const* vib, float* dst, int n, int ch);
