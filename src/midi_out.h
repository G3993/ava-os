// AVA OS — CoreMIDI virtual source: the octagon (and tuner) as a MIDI
// controller any host can subscribe to — Wavetuner in a browser, a DAW, a
// hardware synth via a USB interface. Appears as "AVA OS Octagon".
#pragma once
#include <atomic>
#include <cstdint>

class MidiOut {
public:
    bool start();
    void stop();
    void noteOn(int ch, int note, int vel);
    void noteOff(int ch, int note);
    void cc(int ch, int cc, int val);
    void pitchBend(int ch, int val14); // 0..16383, 8192 = centre
    void pressure(int ch, int val);    // channel aftertouch
    bool running() const { return src_ != nullptr; }
    std::atomic<uint32_t> activity{0}; // bumps on every message sent

private:
    void send(const uint8_t* b, int n);
    void* client_ = nullptr;
    void* src_ = nullptr;
};
