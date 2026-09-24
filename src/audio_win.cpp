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
    Engine* eng = self->engine();
    int n = (int)frames;

    static thread_local std::vector<float> L, R, vib[NZONES];
    L.resize(n); R.resize(n);
    float* vibPtr[NZONES];
    for (int z = 0; z < NZONES; z++) { vib[z].resize(n); vibPtr[z] = vib[z].data(); }

    // latency control: keep the tap→output queue tight (see audio_out.cpp)
    {
        StereoRing* ring = self->ring();
        int target = n + 512;
        int avail = ring->available();
        if (avail > target + 2 * n) ring->discard(avail - target);
    }
    self->ring()->pop(L.data(), R.data(), n);
    if (self->player && self->player->active()) {
        static thread_local std::vector<float> stem[NZONES];
        float* stemPtr[NZONES];
        for (int z = 0; z < NZONES; z++) { stem[z].resize(n); stemPtr[z] = stem[z].data(); }
        if (self->player->render(L.data(), R.data(), stemPtr, n)) {
            eng->process(L.data(), R.data(), vibPtr, n);
            mixOutputBlock(self, eng, L.data(), R.data(), stemPtr, nullptr, (float*)output, n,
                           (int)dev->playback.channels);
            return;
        }
    }
    eng->process(L.data(), R.data(), vibPtr, n);
    eng->delayMusic(L.data(), R.data(), L.data(), R.data(), n);
    const float* vibR[NZONES];
    for (int z = 0; z < NZONES; z++) vibR[z] = eng->rightOut(z);

    mixOutputBlock(self, eng, L.data(), R.data(), vibPtr, vibR, (float*)output, n,
                   (int)dev->playback.channels);
}

bool OutputUnit::start(unsigned deviceID, Engine* engine, StereoRing* ring) {
    stop();
    engine_ = engine;
    ring_ = ring;
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

#endif // !__APPLE__

// hardware volume scalars are a CoreAudio concept; Windows mixer volume is
// left to the OS (the HEALTH panel simply hides the MAX HW control)
float deviceHwVolume(unsigned) { return -1.0f; }
bool maxDeviceHwVolume(unsigned) { return false; }
