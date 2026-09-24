// AVA OS — stem player: plays an authored multichannel set straight to the
// bed. A set is a folder of WAV stems named by zone (the ADAM SETS
// convention "_ch1+2_hp / _ch3_HEAD / _ch4_HEART / _ch5_BELLY / _ch6_ROOT /
// _ch7_FEET", or Ableton track exports named head / heart / belly / root /
// feet / master). Stems stream from disk on a reader thread; the audio
// thread pulls sample-aligned frames for the music pair and the five zones.
// While a set is playing, the live engine's vibration is replaced by the
// stems (the engine still analyses the music for the visuals).
#pragma once
#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include <cstdio>
#include "ringbuf.h"

class StemPlayer {
public:
    static constexpr int kStems = 6;   // 0 = headphones (stereo), 1..5 = HEAD HEART BELLY ROOT FEET
    static constexpr int kSR = 48000;

    ~StemPlayer();
    // load every stem found in a folder (or the folder of a dropped file);
    // false + lastError if nothing usable. Stops playback first.
    bool load(const std::string& folderOrFile);
    void unload();
    bool loaded() const { return frames_ > 0; }

    // transport (UI thread)
    void play();
    void pause();
    void stop();                       // back to the start, live engine takes over
    void seek(double sec);
    bool playing() const { return state_.load() == 1; }
    bool paused() const { return state_.load() == 2; }
    bool active() const { return loaded() && state_.load() != 0; }
    double positionSec() const { return playHead_.load() / (double)kSR; }
    double lengthSec() const { return frames_ / (double)kSR; }
    double leadInSec() const { return leadIn_ / (double)kSR; }

    // audio thread: writes n frames of the music pair and the five zones.
    // Returns false when the player is not active (caller renders live).
    bool render(float* hpL, float* hpR, float* const* zones, int n);

    std::string name, lastError;
    bool has[kStems] = {false, false, false, false, false, false};
    std::string stemFile[kStems];
    std::atomic<float> stemPeak[kStems];   // UI meters (audio thread, decayed)
    std::atomic<bool> filling{false};      // reader is refilling after a seek

private:
    struct Stem {
        FILE* f = nullptr;
        long dataOff = 0, frames = 0;
        int ch = 0, bits = 0, fmt = 0, sr = 0, blockAlign = 0;
        long readPos = 0;
    };
    bool openWav(const std::string& path, Stem& s);
    void readerLoop();
    void fillStem(int i, int wantFrames);
    void decodeInto(Stem& s, const unsigned char* raw, int frames, std::vector<float>& lr);

    Stem stems_[kStems];
    StereoRing rings_[kStems];
    std::vector<unsigned char> raw_;
    std::vector<float> dec_;
    long frames_ = 0, leadIn_ = 0;
    std::atomic<long> playHead_{0};
    std::atomic<long> seekTo_{-1};
    std::atomic<int> state_{0};        // 0 stopped · 1 playing · 2 paused
    std::atomic<bool> quit_{false};
    std::thread reader_;
};
