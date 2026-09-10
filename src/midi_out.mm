#include "midi_out.h"
#import <CoreMIDI/CoreMIDI.h>
#include <algorithm>

static inline int clamp7(int v) { return std::max(0, std::min(127, v)); }

bool MidiOut::start() {
    MIDIClientRef client = 0;
    if (MIDIClientCreate(CFSTR("AVA OS Out"), nullptr, nullptr, &client) != noErr)
        return false;
    MIDIEndpointRef src = 0;
    if (MIDISourceCreate(client, CFSTR("AVA OS Octagon"), &src) != noErr) {
        MIDIClientDispose(client);
        return false;
    }
    // stable unique ID so hosts keep their mappings across launches
    MIDIObjectSetIntegerProperty(src, kMIDIPropertyUniqueID, 0x41564131);
    client_ = (void*)(uintptr_t)client;
    src_ = (void*)(uintptr_t)src;
    return true;
}

void MidiOut::stop() {
    if (src_) MIDIEndpointDispose((MIDIEndpointRef)(uintptr_t)src_);
    if (client_) MIDIClientDispose((MIDIClientRef)(uintptr_t)client_);
    src_ = client_ = nullptr;
}

void MidiOut::send(const uint8_t* b, int n) {
    if (!src_) return;
    Byte buf[64];
    MIDIPacketList* pl = (MIDIPacketList*)buf;
    MIDIPacket* p = MIDIPacketListInit(pl);
    p = MIDIPacketListAdd(pl, sizeof(buf), p, 0, (ByteCount)n, b);
    if (!p) return;
    if (MIDIReceived((MIDIEndpointRef)(uintptr_t)src_, pl) == noErr)
        activity.fetch_add(1, std::memory_order_relaxed);
}

void MidiOut::noteOn(int ch, int note, int vel) {
    uint8_t b[3] = {(uint8_t)(0x90 | (ch & 15)), (uint8_t)clamp7(note), (uint8_t)std::max(1, clamp7(vel))};
    send(b, 3);
}
void MidiOut::noteOff(int ch, int note) {
    uint8_t b[3] = {(uint8_t)(0x80 | (ch & 15)), (uint8_t)clamp7(note), 0};
    send(b, 3);
}
void MidiOut::cc(int ch, int cc, int val) {
    uint8_t b[3] = {(uint8_t)(0xB0 | (ch & 15)), (uint8_t)clamp7(cc), (uint8_t)clamp7(val)};
    send(b, 3);
}
void MidiOut::pitchBend(int ch, int val14) {
    val14 = std::max(0, std::min(16383, val14));
    uint8_t b[3] = {(uint8_t)(0xE0 | (ch & 15)), (uint8_t)(val14 & 0x7F), (uint8_t)(val14 >> 7)};
    send(b, 3);
}
void MidiOut::pressure(int ch, int val) {
    uint8_t b[2] = {(uint8_t)(0xD0 | (ch & 15)), (uint8_t)clamp7(val)};
    send(b, 2);
}
