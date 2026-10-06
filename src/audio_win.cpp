// Windows audio backend: implements the SystemTap and OutputUnit interfaces
// with miniaudio/WASAPI.
//   capture: WASAPI loopback of the default playback device (the Windows
//            equivalent of the macOS process tap; plays-and-captures, so keep
//            the app's own output routed to a *different* device than the
//            Windows default or it will feed back — see README)
//   output:  WASAPI render on the chosen device, same channel layout as the
//            macOS AUHAL path (music pair + one channel per zone)
#ifndef __APPLE__

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#include "miniaudio.h"

#include <vector>
#include <cmath>
#include <cstring>
#include "capture_tap.h"
#include "audio_out.h"
#include "audio_in.h"

// ── shared context + device cache ──
static ma_context gCtx;
static bool gCtxInit = false;
static std::vector<ma_device_id> gDevIds; // index-aligned with listOutputDevices

static bool ensureContext() {
    if (gCtxInit) return true;
    if (ma_context_init(nullptr, 0, nullptr, &gCtx) != MA_SUCCESS) return false;
    gCtxInit = true;
    return true;
}

unsigned defaultOutputDevice() { return 0; } // miniaudio: index 0 is the default playback device

std::vector<OutDevice> listOutputDevices() {
    std::vector<OutDevice> out;
    if (!ensureContext()) return out;

    ma_device_info* infos = nullptr;
    ma_uint32 count = 0;
    if (ma_context_get_devices(&gCtx, &infos, &count, nullptr, nullptr) != MA_SUCCESS)
        return out;

    gDevIds.clear();
    for (ma_uint32 i = 0; i < count; i++) {
        ma_device_info full = infos[i];
        // full info (incl. native formats) needs an explicit query
        ma_context_get_device_info(&gCtx, ma_device_type_playback, &infos[i].id, &full);
        int ch = 0;
        double rate = 48000;
        for (ma_uint32 f = 0; f < full.nativeDataFormatCount; f++) {
            ch = ch > (int)full.nativeDataFormats[f].channels
                     ? ch : (int)full.nativeDataFormats[f].channels;
            if (full.nativeDataFormats[f].sampleRate > 0)
                rate = full.nativeDataFormats[f].sampleRate;
        }
        if (ch == 0) ch = 2;
        gDevIds.push_back(infos[i].id);
        out.push_back({(unsigned)i, full.name, ch, rate});
    }
    return out;
}

// ── system-audio capture (WASAPI loopback) ──
static void tapDataCB(ma_device* dev, void*, const void* input, ma_uint32 frames) {
    auto* self = (SystemTap*)dev->pUserData;
    const float* data = (const float*)input;
    if (!data || frames == 0) return;

    float peak = 0;
    for (ma_uint32 i = 0; i < frames * 2; i++)
        peak = std::max(peak, std::fabs(data[i]));
    self->inputPeak.store(std::max(peak, self->inputPeak.load() * 0.9f));

    if (self->ring()) self->ring()->push(data, (int)frames);
}

bool SystemTap::start(StereoRing* ring) {
    if (running_.load()) return true;
    ring_ = ring;
    lastError.clear();
    if (!ensureContext()) { lastError = "audio context init failed"; return false; }

    ma_device_config cfg = ma_device_config_init(ma_device_type_loopback);
    cfg.capture.pDeviceID = nullptr; // default playback device's loopback
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 2;
    cfg.sampleRate = 48000; // miniaudio resamples if the device differs
    cfg.dataCallback = tapDataCB;
    cfg.pUserData = this;

    auto* dev = new ma_device;
    if (ma_device_init(&gCtx, &cfg, dev) != MA_SUCCESS) {
        delete dev;
        lastError = "loopback capture init failed";
        return false;
    }
    if (ma_device_start(dev) != MA_SUCCESS) {
        ma_device_uninit(dev);
        delete dev;
        lastError = "loopback capture start failed";
        return false;
    }
    tapSampleRate = 48000.0;
    procID_ = dev; // reuse the opaque slot for the ma_device
    running_.store(true);
    return true;
}

void SystemTap::stop() {
    if (procID_) {
        auto* dev = (ma_device*)procID_;
        ma_device_uninit(dev);
        delete dev;
        procID_ = nullptr;
    }
    running_.store(false);
}

// ── output (WASAPI render, mirrors the macOS AUHAL renderCB) ──
static void outDataCB(ma_device* dev, void* output, const void*, ma_uint32 frames) {
    auto* self = (OutputUnit*)dev->pUserData;
    // WASAPI blocks are coarser than CoreAudio's, so the tap queue is kept
    // 512 frames deep instead of 128
    renderOutputBlock(self, self->engine(), (float*)output, (int)frames, (int)dev->playback.channels, (int)frames + 512);
}

bool OutputUnit::start(unsigned deviceID, Engine* engine, StereoRing* ring) {
    stop();
    engine_ = engine;
    ring_ = ring;
    for (int z = 0; z < NZONES; z++) bedDelay_[z].init(16384);
    lastError.clear();
    if (!ensureContext()) { lastError = "audio context init failed"; return false; }

    int devCh = 2;
    auto devs = listOutputDevices(); // also refreshes gDevIds
    if (deviceID >= gDevIds.size()) { lastError = "bad device index"; return false; }
    for (auto& d : devs) if (d.id == deviceID) devCh = d.channels;
    activeChannels = devCh < OutputUnit::kMaxCh ? devCh : OutputUnit::kMaxCh;

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID = &gDevIds[deviceID];
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = (ma_uint32)activeChannels;
    cfg.sampleRate = (ma_uint32)Engine::kSR;
    cfg.periodSizeInFrames = 256; // low latency, matches the macOS buffer
    cfg.dataCallback = outDataCB;
    cfg.pUserData = this;

    auto* dev = new ma_device;
    if (ma_device_init(&gCtx, &cfg, dev) != MA_SUCCESS) {
        delete dev;
        lastError = "output device init failed";
        return false;
    }
    if (ma_device_start(dev) != MA_SUCCESS) {
        ma_device_uninit(dev);
        delete dev;
        lastError = "output device start failed";
        return false;
    }
    unit_ = dev;
    running_.store(true);
    return true;
}

void OutputUnit::stop() {
    if (unit_) {
        auto* dev = (ma_device*)unit_;
        ma_device_uninit(dev);
        delete dev;
        unit_ = nullptr;
    }
    running_.store(false);
}

// ── live input + live set (capture devices via miniaudio/WASAPI) ──
// Input device ids are 1000 + the capture-list index, so they never collide
// with the playback indices listOutputDevices() hands out.
static std::vector<ma_device_id> gInDevIds;
static const unsigned kInIdBase = 1000;

std::vector<OutDevice> listInputDevices() {
    std::vector<OutDevice> out;
    if (!ensureContext()) return out;
    ma_device_info* pb = nullptr; ma_uint32 npb = 0;
    ma_device_info* cap = nullptr; ma_uint32 ncap = 0;
    if (ma_context_get_devices(&gCtx, &pb, &npb, &cap, &ncap) != MA_SUCCESS) return out;
    gInDevIds.clear();
    for (ma_uint32 i = 0; i < ncap; i++) {
        ma_device_info full = cap[i];
        ma_context_get_device_info(&gCtx, ma_device_type_capture, &cap[i].id, &full);
        int ch = 0; double rate = 48000;
        for (ma_uint32 f = 0; f < full.nativeDataFormatCount; f++) {
            ch = std::max(ch, (int)full.nativeDataFormats[f].channels);
            if (full.nativeDataFormats[f].sampleRate > 0) rate = full.nativeDataFormats[f].sampleRate;
        }
        if (ch == 0) ch = 2;
        gInDevIds.push_back(cap[i].id);
        out.push_back({kInIdBase + i, full.name, ch, rate});
    }
    return out;
}

unsigned defaultInputDevice() {
    if (!ensureContext()) return 0;
    ma_device_info* pb = nullptr; ma_uint32 npb = 0;
    ma_device_info* cap = nullptr; ma_uint32 ncap = 0;
    if (ma_context_get_devices(&gCtx, &pb, &npb, &cap, &ncap) != MA_SUCCESS || ncap == 0) return 0;
    for (ma_uint32 i = 0; i < ncap; i++) if (cap[i].isDefault) return kInIdBase + i;
    return kInIdBase;
}

// Windows has no per-app microphone prompt the way macOS does: the privacy
// switch lives in Settings and a blocked mic simply captures silence.
int micAuthStatus() { return 2; }
void micRequestAccess() {}

static bool inputIndex(unsigned deviceID, size_t& idx) {
    if (deviceID < kInIdBase) return false;
    idx = deviceID - kInIdBase;
    return idx < gInDevIds.size();
}

static void inDataCB(ma_device* dev, void*, const void* input, ma_uint32 frames) {
    auto* self = (InputUnit*)dev->pUserData;
    self->cbCount.fetch_add(1, std::memory_order_relaxed);
    if (!input || frames == 0) return;
    self->processBlock((const float*)input, self->deviceChannels, (int)frames);
}

bool InputUnit::start(unsigned deviceID, StereoRing* ring) {
    stop();
    ring_ = ring;
    lastError.clear();
    if (!ensureContext()) { lastError = "audio context init failed"; return false; }
    auto ins = listInputDevices();   // refreshes gInDevIds
    size_t idx;
    if (!inputIndex(deviceID, idx)) { lastError = "no such input device"; return false; }
    deviceChannels = std::min(32, std::max(1, ins[idx].channels));
    hwRate = ins[idx].sampleRate;
    cbCount.store(0); lastErr.store(0);
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &gInDevIds[idx];
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = (ma_uint32)deviceChannels;
    cfg.sampleRate = 48000;           // miniaudio resamples if the device differs
    cfg.periodSizeInFrames = 128;
    cfg.dataCallback = inDataCB;
    cfg.pUserData = this;
    auto* dev = new ma_device;
    if (ma_device_init(&gCtx, &cfg, dev) != MA_SUCCESS) { delete dev; lastError = "input device init failed"; return false; }
    if (ma_device_start(dev) != MA_SUCCESS) { ma_device_uninit(dev); delete dev; lastError = "input start failed"; return false; }
    unit_ = dev;
    running_.store(true);
    return true;
}

void InputUnit::stop() {
    if (!unit_) return;
    auto* dev = (ma_device*)unit_;
    ma_device_uninit(dev);
    delete dev;
    unit_ = nullptr;
    running_.store(false);
    peak.store(0);
}

static void bedDataCB(ma_device* dev, void*, const void* input, ma_uint32 frames) {
    auto* self = (BedInput*)dev->pUserData;
    if (!input || frames == 0) return;
    const float* src = (const float*)input;
    int ch = self->deviceChannels;
    int first = std::min(std::max(self->firstChan.load(), 0), std::max(0, ch - 1));
    self->ring.pushInterleaved(src, (int)frames, ch, first);
    for (int c = 0; c < BedInput::kCh; c++) {
        int sc = first + c;
        float pk = 0;
        if (sc < ch) for (ma_uint32 i = 0; i < frames; i++) pk = std::max(pk, std::fabs(src[i * ch + sc]));
        float prev = self->peak[c].load(std::memory_order_relaxed) * 0.9f;
        self->peak[c].store(pk > prev ? pk : prev, std::memory_order_relaxed);
    }
}

bool BedInput::start(unsigned devID) {
    stop();
    lastError.clear();
    deviceID = devID;
    if (!ensureContext()) { lastError = "audio context init failed"; return false; }
    auto ins = listInputDevices();
    size_t idx;
    if (!inputIndex(devID, idx)) { lastError = "no such input device"; return false; }
    deviceChannels = std::min(32, std::max(1, ins[idx].channels));
    if (deviceChannels < 2) { lastError = "device has fewer than 2 inputs"; return false; }
    ring.init(48000, kCh);
    for (int c = 0; c < kCh; c++) peak[c].store(0.0f);
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &gInDevIds[idx];
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = (ma_uint32)deviceChannels;
    cfg.sampleRate = 48000;
    cfg.periodSizeInFrames = 128;
    cfg.dataCallback = bedDataCB;
    cfg.pUserData = this;
    auto* dev = new ma_device;
    if (ma_device_init(&gCtx, &cfg, dev) != MA_SUCCESS) { delete dev; lastError = "input device init failed"; return false; }
    if (ma_device_start(dev) != MA_SUCCESS) { ma_device_uninit(dev); delete dev; lastError = "input start failed"; return false; }
    unit_ = dev;
    setRunning(true);
    return true;
}

void BedInput::stop() {
    if (!unit_) return;
    setRunning(false);
    auto* dev = (ma_device*)unit_;
    ma_device_uninit(dev);
    delete dev;
    unit_ = nullptr;
    for (int c = 0; c < kCh; c++) peak[c].store(0.0f);
}

#endif // !__APPLE__

// hardware volume scalars are a CoreAudio concept; Windows mixer volume is
// left to the OS (the HEALTH panel simply hides the MAX HW control)
float deviceHwVolume(unsigned) { return -1.0f; }
bool maxDeviceHwVolume(unsigned) { return false; }
