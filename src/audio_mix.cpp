// Shared output mixer for every backend (CoreAudio on macOS, miniaudio on
// Windows): routing, solo, second sends, thermal guard, channel-finder ping
// and per-channel meters. Backends only pull the tapped stereo, run the
// engine, and hand the interleaved device buffer here.
#include "audio_out.h"
#include <algorithm>
#include <cmath>
#include <cstring>

void mixOutputBlock(OutputUnit* self, Engine* eng, const float* L, const float* R,
                    float* const* vib, float* dst, int n, int ch) {
    float vol = eng->params.masterVolume.load();
    bool monitor = eng->params.monitorVibOnStereo.load() != 0;

    // Routing: music pair + surround pair + per-zone channels, all on the
    // chosen device. On a plain stereo device (≤2ch) fall back to a vibration
    // monitor mix instead of doubling the music that's usually audible there.
    int musicL = eng->params.musicChanL.load();
    float musicG = eng->params.musicGain.load();
    int surrL = eng->params.surrChanL.load();
    float surrG = eng->params.surrGain.load();
    float surrW = eng->params.surrWidth.load();
    int zc[NZONES], zc2[NZONES];
    float zt[NZONES];
    int solo = eng->params.soloZone.load();
    for (int z = 0; z < NZONES; z++) {
        zc[z] = eng->params.zoneChan[z].load();
        zc2[z] = eng->params.zoneChan2[z].load();
        zt[z] = (solo >= 0 && z != solo) ? 0.0f : eng->params.zoneTrim[z].load();
    }

    // block peak per device channel, decayed into chanPeak for the monitor
    auto captureMeters = [&]() {
        for (int c = 0; c < ch && c < OutputUnit::kMaxCh; c++) {
            float pk = 0;
            for (int i = 0; i < n; i++) {
                float a = std::fabs(dst[(size_t)i * ch + c]);
                if (a > pk) pk = a;
            }
            float prev = self->chanPeak[c].load(std::memory_order_relaxed) * 0.92f;
            self->chanPeak[c].store(pk > prev ? pk : prev, std::memory_order_relaxed);
        }
    };

    std::memset(dst, 0, (size_t)n * ch * sizeof(float));
    if (ch <= 2) {
        for (int i = 0; i < n; i++) {
            float v = monitor ? 0.7f * 0.25f * (vib[HEART][i] + vib[BELLY][i] +
                                                vib[ROOT][i] + vib[FEET][i]) : 0.0f;
            dst[i * ch] = vol * v;
            if (ch > 1) dst[i * ch + 1] = vol * v;
        }
        captureMeters();
        return;
    }
    for (int i = 0; i < n; i++) {
        float* f = dst + (size_t)i * ch;
        if (musicL >= 0 && musicL < ch) {
            f[musicL] += vol * musicG * L[i];
            if (musicL + 1 < ch) f[musicL + 1] += vol * musicG * R[i];
        }
        if (surrL >= 0 && surrL < ch) {
            // mid/side width control: 0 = mono fill, 1 = full stereo image
            float mid = 0.5f * (L[i] + R[i]);
            float side = 0.5f * (L[i] - R[i]) * surrW;
            f[surrL] += vol * surrG * (mid + side);
            if (surrL + 1 < ch) f[surrL + 1] += vol * surrG * (mid - side);
        }
        for (int z = 0; z < NZONES; z++) {
            if (zc[z] >= 0 && zc[z] < ch) f[zc[z]] += vol * zt[z] * vib[z][i];
            if (zc2[z] >= 0 && zc2[z] < ch) f[zc2[z]] += vol * zt[z] * vib[z][i];
        }
    }
    // thermal guard on zone channels: integrate output power over ~45 s and,
    // past the RMS budget, ease the channel down (2 s slew) until it cools
    {
        float lim = eng->params.thermalLimit.load();
        bool zoneCh[OutputUnit::kMaxCh] = {false};
        for (int z = 0; z < NZONES; z++) {
            if (zc[z] >= 0 && zc[z] < OutputUnit::kMaxCh) zoneCh[zc[z]] = true;
            if (zc2[z] >= 0 && zc2[z] < OutputUnit::kMaxCh) zoneCh[zc2[z]] = true;
        }
        const float aHeat = 1.0f / (45.0f * 48000.0f);
        const float aG = 1.0f / (2.0f * 48000.0f);
        const float budget = lim * lim;
        for (int c = 0; c < ch && c < OutputUnit::kMaxCh; c++) {
            if (!zoneCh[c]) { self->chanHeat[c].store(0, std::memory_order_relaxed); continue; }
            float ms = self->heatMs_[c], g = self->thermG_[c];
            for (int i = 0; i < n; i++) {
                float& v = dst[(size_t)i * ch + c];
                ms += (v * v - ms) * aHeat; // demand (pre-gain) power, so the cap is exact
                float tg = 1.0f;
                if (lim > 0.0f && ms > budget) tg = std::max(0.2f, std::sqrt(budget / ms));
                g += (tg - g) * aG;
                v *= g;
            }
            self->heatMs_[c] = ms;
            self->thermG_[c] = g;
            self->chanHeat[c].store(budget > 0 ? ms / budget : 0, std::memory_order_relaxed);
            self->chanThermGain[c].store(g, std::memory_order_relaxed);
        }
    }
    // channel-finder burst: 55 Hz sine with 30 ms fades on one raw channel
    {
        int pc = self->pingChan.load(std::memory_order_relaxed);
        int left = self->pingLeft_.load(std::memory_order_relaxed);
        if (pc >= 0 && pc < ch && left > 0) {
            const int total = 43200, fade = 1440;
            for (int i = 0; i < n && left > 0; i++, left--) {
                int done = total - left;
                float env = std::min(1.0f, std::min(done, left) / (float)fade);
                dst[(size_t)i * ch + pc] += 0.7f * env * std::sin(self->pingPhase_);
                self->pingPhase_ += 2.0f * 3.14159265f * 55.0f / 48000.0f;
                if (self->pingPhase_ > 6.2831853f) self->pingPhase_ -= 6.2831853f;
            }
            self->pingLeft_.store(left, std::memory_order_relaxed);
            if (left <= 0) self->pingChan.store(-1, std::memory_order_relaxed);
        } else if (pc >= 0 && left <= 0) {
            self->pingChan.store(-1, std::memory_order_relaxed);
        }
    }
    captureMeters();
}
