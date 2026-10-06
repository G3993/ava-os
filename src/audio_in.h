// Live input: pull channels from an input device (the interface's inputs,
// or the Mac's / PC's mic) and feed them to the engine alongside the
// system-audio tap. Capture is per platform (macOS AUHAL in audio_in.mm,
// miniaudio/WASAPI in audio_win.cpp); the per-block channel pick, gain,
// clip/loop guard and metering are shared (audio_in_common.cpp).
#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "ringbuf.h"
#include "audio_out.h"
#include "bed_input.h"

// every CoreAudio device with input channels (name, channel count, rate)
std::vector<OutDevice> listInputDevices();
// the OS's current default input (the Mac's mic unless the user chose
// otherwise); 0 if unknown
unsigned defaultInputDevice();
// macOS microphone privacy: 0 = not asked yet, 1 = denied/restricted, 2 = allowed.
// Every audio input (an interface too) runs through this permission; when
// it is denied CoreAudio hands back silence without an error.
int micAuthStatus();
void micRequestAccess();

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
    std::atomic<long>  cbCount{0};   // diagnostics: input callbacks seen, last AudioUnitRender status
    std::atomic<int>   lastErr{0};
    double hwRate = 0;               // the device's own input sample rate
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
    // audio thread, both platforms: interleaved float frames of `ch`
    // channels → L/R pick, gain, guard, peak, into the ring
    void processBlock(const float* interleaved, int ch, int nFrames);
    void* unit_ = nullptr;
    std::vector<float> scratch_;
private:
    std::atomic<bool> running_{false};
    StereoRing* ring_ = nullptr;
};
