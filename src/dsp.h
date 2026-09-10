// AVA OS — small DSP primitives (biquads, followers, oscillators)
#pragma once
#include <cmath>
#include <cstring>
#include <vector>

namespace dsp {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 2.0f * kPi;

// ── Biquad (RBJ cookbook) ────────────────────────────────────────────────
struct Biquad {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    inline float process(float x) {
        float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0; }

    void lowpass(float fc, float sr, float q = 0.70710678f) {
        float w = kTwoPi * fc / sr, cw = std::cos(w), sw = std::sin(w);
        float alpha = sw / (2 * q), a0 = 1 + alpha;
        b0 = (1 - cw) / 2 / a0; b1 = (1 - cw) / a0; b2 = b0;
        a1 = -2 * cw / a0; a2 = (1 - alpha) / a0;
    }
    void highpass(float fc, float sr, float q = 0.70710678f) {
        float w = kTwoPi * fc / sr, cw = std::cos(w), sw = std::sin(w);
        float alpha = sw / (2 * q), a0 = 1 + alpha;
        b0 = (1 + cw) / 2 / a0; b1 = -(1 + cw) / a0; b2 = b0;
        a1 = -2 * cw / a0; a2 = (1 - alpha) / a0;
    }
};

// Butterworth cascade: N-order lowpass as N/2 biquads with butterworth Qs
struct ButterLP {
    Biquad s[3];
    int nSections = 3;
    void design(float fc, float sr, int order = 6) {
        nSections = order / 2;
        for (int k = 0; k < nSections; k++) {
            // Butterworth pole Q values for cascaded biquads
            float q = 1.0f / (2.0f * std::cos(kPi * (2.0f * k + 1.0f) / (2.0f * order)));
            s[k].lowpass(fc, sr, q);
        }
    }
    inline float process(float x) {
        for (int k = 0; k < nSections; k++) x = s[k].process(x);
        return x;
    }
    void reset() { for (auto& b : s) b.reset(); }
};

// 4th-order bandpass built from HP + LP butterworth pairs
struct BandPass {
    Biquad hp1, hp2, lp1, lp2;
    void design(float lo, float hi, float sr) {
        hp1.highpass(lo, sr, 0.54119610f); hp2.highpass(lo, sr, 1.30656296f);
        lp1.lowpass(hi, sr, 0.54119610f);  lp2.lowpass(hi, sr, 1.30656296f);
    }
    inline float process(float x) {
        return lp2.process(lp1.process(hp2.process(hp1.process(x))));
    }
};

// Envelope follower: rectify → attack/release one-pole → running-max AGC
struct EnvFollower {
    float aCoef = 0, rCoef = 0, env = 0;
    float runMax = 1e-4f, maxDecay = 0;
    void design(float attackMs, float releaseMs, float sr, float maxDecaySec = 10.0f) {
        aCoef = std::exp(-1.0f / (attackMs * 0.001f * sr));
        rCoef = std::exp(-1.0f / (releaseMs * 0.001f * sr));
        maxDecay = std::exp(-1.0f / (maxDecaySec * sr));
    }
    // returns normalized 0..1 envelope (offline engine normalizes to global max;
    // we track a slowly-decaying running max instead)
    inline float process(float x) {
        float r = std::fabs(x);
        env = (r > env ? aCoef : rCoef) * (env - r) + r;
        runMax = std::max(env, runMax * maxDecay);
        return runMax > 1e-5f ? env / runMax : 0.0f;
    }
};

// Phase accumulator oscillator
struct Osc {
    float phase = 0;
    inline float tick(float freq, float sr) {
        float v = std::sin(phase);
        phase += kTwoPi * freq / sr;
        if (phase > kTwoPi) phase -= kTwoPi;
        return v;
    }
    // sin at integer harmonic of current phase (phase-coherent)
    inline float harmonic(int k) const { return std::sin(phase * (float)k); }
};

// One-pole parameter smoother
struct Smooth {
    float v = 0, coef = 0;
    void design(float tauSec, float sr) { coef = std::exp(-1.0f / (tauSec * sr)); }
    inline float tick(float target) { v = coef * (v - target) + target; return v; }
};

// Simple delay line with taps
struct DelayLine {
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init(int sizePow2) { buf.assign(sizePow2, 0.0f); mask = sizePow2 - 1; w = 0; }
    inline void push(float x) { buf[w] = x; w = (w + 1) & mask; }
    inline float tap(int delay) const { return buf[(w - 1 - delay) & mask]; }
};

} // namespace dsp
