// AVA OS — CoreMIDI input: any pad controller becomes a bed instrument.
// Connects every source, hot-plugs via cheap rescan, parses note/CC/bend/
// aftertouch (running status included) into a queue drained on the UI thread.
#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct MidiEvent {
    uint8_t type;    // 0x90 noteOn (b>0), 0x80 noteOff, 0xB0 CC, 0xE0 bend, 0xD0 chan AT
    uint8_t channel; // 0-15
    uint8_t a, b;    // note/vel · cc/val · bend lsb/msb · pressure/0
};

class MidiIn {
public:
    bool start();
    void stop();
    // hot-plug: connect sources that appeared since last call (call ~every 2 s)
    void rescan();
    // drain pending events (UI thread)
    std::vector<MidiEvent> poll();

    bool connected() const { return nConnected_.load() > 0; }
    std::string deviceName(); // first source's display name, "" if none
    std::atomic<uint32_t> activity{0}; // bumps on every parsed event

    // internal (MIDI thread)
    void push(const MidiEvent& e);

private:
    void* client_ = nullptr;
    void* port_ = nullptr;
    std::mutex mu_;
    std::vector<MidiEvent> q_;
    std::vector<uint32_t> connected_; // MIDIEndpointRef values already connected
    std::atomic<int> nConnected_{0};
    std::string name_; // guarded by mu_
};
