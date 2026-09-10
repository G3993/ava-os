#include "midi_in.h"
#import <CoreMIDI/CoreMIDI.h>

// Parse raw MIDI bytes with running-status support (cheap controllers and
// DIN bridges omit repeated status bytes; without this they lose every
// message after the first).
static void parseBytes(MidiIn* self, const uint8_t* d, int len) {
    static thread_local uint8_t runStatus = 0;
    int j = 0;
    while (j < len) {
        uint8_t st = d[j];
        if (st >= 0xF8) { j++; continue; }          // realtime, ignore
        if (st >= 0xF0) { runStatus = 0; j++; continue; } // system common: skip
        if (st >= 0x80) { runStatus = st; j++; }
        else if (!runStatus) { j++; continue; }      // data byte w/o status
        uint8_t msg = runStatus & 0xF0, chan = runStatus & 0x0F;
        int need = (msg == 0xC0 || msg == 0xD0) ? 1 : 2;
        if (len - j < need) return;
        MidiEvent ev{};
        ev.channel = chan;
        ev.a = d[j] & 0x7F;
        ev.b = need > 1 ? (d[j + 1] & 0x7F) : 0;
        j += need;
        switch (msg) {
            case 0x90: ev.type = ev.b > 0 ? 0x90 : 0x80; break; // vel 0 = off
            case 0x80: ev.type = 0x80; break;
            case 0xB0: ev.type = 0xB0; break;
            case 0xE0: ev.type = 0xE0; break;
            case 0xD0: ev.type = 0xD0; break;
            case 0xA0: ev.type = 0xD0; ev.a = d[j - 1]; break; // poly AT → chan AT
            default: continue; // program change etc.
        }
        self->push(ev);
    }
}

static void midiReadProc(const MIDIPacketList* pktList, void* refCon, void*) {
    auto* self = (MidiIn*)refCon;
    const MIDIPacket* packet = &pktList->packet[0];
    for (UInt32 i = 0; i < pktList->numPackets; i++) {
        parseBytes(self, packet->data, packet->length);
        packet = MIDIPacketNext(packet);
    }
}

bool MidiIn::start() {
    MIDIClientRef client = 0;
    if (MIDIClientCreate(CFSTR("AVA OS"), nullptr, nullptr, &client) != noErr)
        return false;
    MIDIPortRef port = 0;
    if (MIDIInputPortCreate(client, CFSTR("AVA In"), midiReadProc, this, &port) != noErr) {
        MIDIClientDispose(client);
        return false;
    }
    client_ = (void*)(uintptr_t)client;
    port_ = (void*)(uintptr_t)port;
    rescan();
    return true;
}

void MidiIn::stop() {
    if (client_) MIDIClientDispose((MIDIClientRef)(uintptr_t)client_);
    client_ = port_ = nullptr;
    connected_.clear();
    nConnected_.store(0);
}

void MidiIn::rescan() {
    if (!port_) return;
    ItemCount n = MIDIGetNumberOfSources();
    std::vector<uint32_t> live;
    std::string firstName;
    for (ItemCount i = 0; i < n; i++) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (!src) continue;
        // never listen to our own "AVA OS Octagon" output — it would feed the
        // octagon's notes straight back into the pad map
        SInt32 uid = 0;
        if (MIDIObjectGetIntegerProperty(src, kMIDIPropertyUniqueID, &uid) == noErr &&
            uid == 0x41564131)
            continue;
        live.push_back((uint32_t)src);
        bool known = false;
        for (uint32_t c : connected_) if (c == (uint32_t)src) known = true;
        if (!known) {
            MIDIPortConnectSource((MIDIPortRef)(uintptr_t)port_, src, nullptr);
            CFStringRef cn = nullptr;
            MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &cn);
            char cb[128] = "?";
            if (cn) { CFStringGetCString(cn, cb, sizeof(cb), kCFStringEncodingUTF8); CFRelease(cn); }
            fprintf(stderr, "[MIDI] connected %s\n", cb);
        }
        if (firstName.empty()) {
            CFStringRef nm = nullptr;
            MIDIObjectGetStringProperty(src, kMIDIPropertyDisplayName, &nm);
            if (nm) {
                char buf[256] = {0};
                CFStringGetCString(nm, buf, sizeof(buf), kCFStringEncodingUTF8);
                firstName = buf;
                CFRelease(nm);
            }
        }
    }
    connected_ = live;
    nConnected_.store((int)live.size());
    std::lock_guard<std::mutex> lk(mu_);
    name_ = firstName;
}

std::vector<MidiEvent> MidiIn::poll() {
    std::lock_guard<std::mutex> lk(mu_);
    auto out = std::move(q_);
    q_.clear();
    return out;
}

std::string MidiIn::deviceName() {
    std::lock_guard<std::mutex> lk(mu_);
    return name_;
}

void MidiIn::push(const MidiEvent& e) {
    activity.fetch_add(1, std::memory_order_relaxed);
    if (e.type == 0x90) fprintf(stderr, "[MIDI] note on %d vel %d\n", e.a, e.b);
    std::lock_guard<std::mutex> lk(mu_);
    if (q_.size() < 1024) q_.push_back(e);
}
