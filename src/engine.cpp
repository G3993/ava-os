#include "engine.h"
#include <algorithm>

void Engine::init() {
    analyzer.init(kSR);

    subBand_.design(20, 60, kSR);
    bassBand_.design(60, 120, kSR);
    lowMidBand_.design(120, 250, kSR);
    midBand_.design(250, 2000, kSR);
    highBand_.design(2000, 8000, kSR);
    rmsPre_.lowpass(4000, kSR);

    rmsF_.design(10, 150, kSR);
    subF_.design(10, 150, kSR);
    bassF_.design(10, 150, kSR);
    lowMidF_.design(10, 150, kSR);
    midF_.design(10, 150, kSR);
    highF_.design(10, 150, kSR);

    freqSm_.design(0.5f, kSR);
    freqSm_.v = 55.0f;

    onsetAtkCoef_ = std::exp(-1.0f / (0.008f * kSR));

    sweepDelay_.init(32768); // > 300 ms @ 48k

    for (int z = 0; z < NZONES; z++) zoneLP_[z].design(200.0f, kSR, 6);
    for (int i = 0; i < NZONES + 2; i++) meter[i].store(0);
    for (int z = 0; z < NZONES; z++) zoneHz[z].store(55.0f);
    for (int z = 0; z < NZONES; z++) {
        padEnv_[z] = 0; padPhase_[z] = 0; padT_[z] = 0; padPrevGate_[z] = 0;
    }
}

void Engine::process(const float* inL, const float* inR, float** out, int n) {
    // snapshot params once per block
    const float intensity = params.intensity.load();
    const float grounding = params.grounding.load();
    const float uplift    = params.uplift.load();
    const float pulseSync = params.pulseSync.load();
    const float bodyFlow  = params.bodyFlow.load();
    const float warmth    = params.waveWarmth.load();
    const float subDepth  = params.subDepth.load();
    const float spread    = params.spread.load();
    const bool  entrain   = params.entrainment.load() != 0;
    const float entHz     = kBrainwaveHz[params.brainwave.load()];
    const float bpm       = analyzer.out.bpm.load();

    float zoneLevel[NZONES];
    const float autoLevels[NZONES] = {
        0.5f + uplift * 0.5f, 0.5f + uplift * 0.3f, 0.8f,
        0.5f + grounding * 0.5f, 0.5f + grounding * 0.5f};
    for (int z = 0; z < NZONES; z++) {
        float o = params.zoneLevel[z].load();
        zoneLevel[z] = o >= 0 ? o : autoLevels[z];
    }

    const float pulseW[NZONES] = {uplift * 0.6f, uplift * 0.8f, 0.7f,
                                  grounding * 0.9f, grounding * 1.0f};

    // module B decay from tempo: tau = decay_s/5 (python exp(-5t/decay))
    float beatDur = 60.0f / std::max(bpm, 60.0f);
    float decayS = std::min(beatDur * 0.5f, 0.4f);
    float onsetDecayCoef = std::exp(-5.0f / (decayS * kSR));

    float sweepRate = 0.2f + bodyFlow * 0.8f;
    int maxDelay = (int)(0.3f * bodyFlow * kSR);

    // breath shaping: exponent 1 (off) .. 3 (strong expansion)
    const float breathAmt = params.breath.load();
    const float breathExp = 1.0f + 2.0f * breathAmt;
    // ~300 ms loudness window; max decays over ~60 s
    const float loudCoef = std::exp(-1.0f / (0.3f * kSR));
    const float maxDecay = 1.0f - 1.0f / (60.0f * kSR);
    // breath gain: sudden breaks duck fast (~150 ms); gradual fades glide
    // (~1.2 s) so a long musical fade-out dims the vibrations gracefully
    const float downCoef = std::exp(-1.0f / (0.15f * kSR));
    const float downSlowCoef = std::exp(-1.0f / (1.2f * kSR));
    const float upCoef = std::exp(-1.0f / (0.8f * kSR));

    // song-character dynamics
    const float dynAmt = params.dynamics.load();
    const float rateDecay = std::exp(-1.0f / (8.0f * kSR));
    const float rhythmCoef = std::exp(-1.0f / (5.0f * kSR));

    // void: negative space carved on the strongest hits of a hot section.
    // Fast in (~50 ms, reads as an event), hold ~1/3 beat, musical release
    // (τ 0.4 beat → mostly back within the beat). The knob widens both gates:
    // low = only the loudest few hits, high = every hard kick is a hole.
    const float voidAmt = params.voidAmt.load();
    const float hotCoef = std::exp(-1.0f / (1.5f * kSR));
    const float bassMaxDecay = 1.0f - 1.0f / (30.0f * kSR);
    const float voidAtkCoef = std::exp(-1.0f / (0.012f * kSR));
    const float voidRelCoef = std::exp(-1.0f / (0.4f * beatDur * kSR));
    const long  voidHoldLen = (long)(0.30f * beatDur * kSR);
    const long  voidRefracLen = (long)(0.45f * beatDur * kSR);
    const float hotTh  = 0.92f - 0.30f * voidAmt;  // sustained-loudness gate
    const float bassTh = 0.92f - 0.45f * voidAmt;  // hit-strength gate
    const float voidDepth = 0.85f + 0.15f * voidAmt;

    float peakAcc[NZONES + 2] = {0};

    // pad play (octagon controller): snapshot per block
    float padHzS[NZONES], padVelS[NZONES];
    int padGateS[NZONES];
    const int patch = std::min(std::max(params.padPatch.load(), 0), NPATCHES - 1);
    const PadPatchDef& pd = kPadPatches[patch];
    for (int z = 0; z < NZONES; z++) {
        padGateS[z] = params.padGate[z].load();
        padHzS[z] = params.padHz[z].load();
        padVelS[z] = params.padVel[z].load();
        if (padGateS[z] && !padPrevGate_[z]) padT_[z] = 0; // new strike
        padPrevGate_[z] = padGateS[z];
    }
    const float padAtk = std::exp(-1.0f / (pd.atkS * kSR));
    const float padRel = std::exp(-1.0f / (pd.relS * kSR));

    // no sound at open: hold silent 0.6 s while followers settle, fade in 0.9 s
    const long warmHold = (long)(0.6f * kSR);
    const long warmRamp = (long)(0.9f * kSR);

    // punch-in FX (momentary): strobe gates at 9 Hz, choke mutes; both ride
    // one smoothed gain (~8 ms) so edges thump instead of clicking
    const int fxStrobeOn = params.fxStrobe.load();
    const int fxChokeOn = params.fxChoke.load();
    const float fxTremD = params.fxTrem.load();
    const float fxCoef = std::exp(-1.0f / (0.008f * kSR));

    for (int i = 0; i < n; i++) {
        float L = inL[i], R = inR[i];
        float mono = 0.5f * (L + R);

        // scope for UI
        bool doScope = false;
        int scopeIdx = 0;
        if (++scopeDecim_ >= 256) {
            scopeDecim_ = 0;
            doScope = true;
            scopeIdx = scopeW.load(std::memory_order_relaxed);
            scope[scopeIdx] = mono;
            scopeW.store((scopeIdx + 1) % kScopeLen, std::memory_order_relaxed);
        }

        // ── breath: macro-dynamics of the song ──
        loudSq_ = loudCoef * (loudSq_ - mono * mono) + mono * mono;
        loudMax_ = std::max(loudSq_, loudMax_ * maxDecay);
        float breathRaw;
        if (loudSq_ < 1e-8f) {
            breathRaw = 0.0f; // true digital silence → vibrations fully rest
        } else {
            float rel = std::sqrt(loudSq_ / std::max(loudMax_, 1e-7f));
            breathRaw = std::pow(std::min(rel, 1.0f), breathExp);
            // soft gate: below rel≈0.05 taper smoothly to zero (no snap)
            float k = rel / 0.05f;
            if (k < 1.0f) breathRaw *= k * k * (3.0f - 2.0f * k);
        }
        float bc;
        if (breathRaw > breathG_) bc = upCoef;                       // swell in
        else if (breathRaw < breathG_ * 0.4f) bc = downCoef;         // hard break
        else bc = downSlowCoef;                                      // gentle fade
        breathG_ = bc * (breathG_ - breathRaw) + breathRaw;

        // ── analysis ──
        float onset = analyzer.pushSample(mono);
        if (onset > 0) onsetTarget_ = std::min(1.0f, onsetTarget_ + onset);

        // ── song character: rhythmic density → adaptive intensity + swell ──
        if (onset > 0) onsetRate_ += 1.0f;
        onsetRate_ *= rateDecay;
        // onsetRate_/8 ≈ onsets per second; ~1.5/s and up = fully rhythmic
        float rhythmTarget = std::min(1.0f, (onsetRate_ / 8.0f) / 1.5f);
        rhythmSm_ = rhythmCoef * (rhythmSm_ - rhythmTarget) + rhythmTarget;
        // ambient → down to ~55% intensity; rhythmic → 100%
        float charGain = 1.0f - dynAmt * 0.45f * (1.0f - rhythmSm_);
        // slow tidal swell (~14 s period): deep for ambient, subtle for beats
        float swellDepth = dynAmt * (0.06f + 0.24f * (1.0f - rhythmSm_));
        float swell = 1.0f - swellDepth * (0.5f + 0.5f * lfoSwell_.tick(0.07f, kSR));

        // onset pulse env: fast attack toward target, tempo-scaled decay
        onsetTarget_ *= onsetDecayCoef;
        onsetEnv_ = onsetAtkCoef_ * (onsetEnv_ - onsetTarget_) + onsetTarget_;

        // envelopes (normalized by running max, mirroring offline normalize)
        float rmsEnv    = rmsF_.process(rmsPre_.process(mono));
        float subEnv    = subF_.process(subBand_.process(mono));
        float bassEnv   = bassF_.process(bassBand_.process(mono));
        float lowMidEnv = lowMidF_.process(lowMidBand_.process(mono));
        float midEnv    = midF_.process(midBand_.process(mono));
        float highEnv   = highF_.process(highBand_.process(mono));
        if (i == n - 1) {
            audioLevel.store(rmsEnv);
            audioBass.store(std::max(subEnv, bassEnv));
            audioMid.store(midEnv);
            audioHigh.store(highEnv);
            audioSubBand.store(subEnv);
            audioLowMidBand.store(lowMidEnv);
            voidNow.store(voidEnv_);
        }

        // ── void: absence as an event — a hot section's hardest bass hits
        // punch holes in the vibration field instead of pushing it louder ──
        {
            float relLoud = std::sqrt(loudSq_ / std::max(loudMax_, 1e-7f));
            hotSm_ = hotCoef * (hotSm_ - relLoud) + relLoud;
            float bassNow = std::max(subEnv, bassEnv);
            bassMax_ = std::max(bassNow, bassMax_ * bassMaxDecay);
            if (voidRefrac_ > 0) voidRefrac_--;
            if (voidHold_ > 0) voidHold_--;
            if (voidAmt > 0.01f && onset > 0 && voidRefrac_ == 0
                && hotSm_ > hotTh && bassNow > bassTh * bassMax_) {
                voidHold_ = voidHoldLen;
                voidRefrac_ = voidRefracLen;
            }
            float vt = voidHold_ > 0 ? 1.0f : 0.0f;
            float vc = vt > voidEnv_ ? voidAtkCoef : voidRelCoef;
            voidEnv_ = vc * (voidEnv_ - vt) + vt;
        }

        // root frequency, smoothed
        float freq = freqSm_.tick(analyzer.out.midOctaveHz.load());
        float subFreq = freq * 0.5f;
        float highFreq = freq * 1.5f;
        float feetFreq = subFreq * 0.5f >= 20.0f ? subFreq * 0.5f : subFreq;

        // oscillators
        float carrier = oscMid_.tick(freq, kSR);
        float sub = oscSub_.tick(subFreq, kSR);
        float high = oscHigh_.tick(highFreq <= 200.0f ? highFreq : freq, kSR);
        float feetOsc = oscFeet_.tick(feetFreq, kSR);

        // warmth: phase-coherent harmonics of the mid carrier (<=200 Hz)
        float warm = carrier, norm = 1.0f;
        if (warmth > 0.01f) {
            for (int k = 2; k < 8; k++) {
                if (freq * k > 200.0f) break;
                float a = warmth * (1.0f / k) * 0.5f;
                warm += a * oscMid_.harmonic(k);
                norm += a;
            }
            warm /= norm;
        }

        // ── module A: tonal foundation ──
        float foundation = (warm * (1.0f - subDepth * 0.5f) + sub * (subDepth * 0.5f)) * rmsEnv;

        // ── module B: transient pulses ──
        float pulses = (carrier * (1.0f - grounding * 0.6f) + sub * (grounding * 0.6f))
                       * onsetEnv_ * pulseSync;

        // ── module C: entrainment (group A vs B, 180°) ──
        float entA = 0, entB = 0;
        {
            float lfo = lfoEnt_.tick(entHz, kSR);
            if (entrain) {
                float la = 0.5f * (1.0f + lfo);
                float lb = 1.0f - la; // 180° offset of a raised sine
                entA = carrier * la * rmsEnv * 0.6f;
                entB = carrier * lb * rmsEnv * 0.6f;
            }
        }

        // ── module D: body sweep with per-zone delay taps ──
        float sweepBase = carrier * 0.5f * (1.0f + lfoSweep_.tick(sweepRate, kSR)) * rmsEnv;
        sweepDelay_.push(sweepBase);

        // ── module E: zone accents ──
        // each zone's accent follows the band that belongs to that body area:
        // HEAD rides melody/air (mid + high) instead of low-mids, HEART blends
        // the bass pump with the vocal/chord band so it tracks the song's
        // emotional center, BELLY stays whole-mix, ROOT/FEET stay sub-locked
        float heartLfo = 0.5f * (1.0f + lfoHeart_.tick(1.1f, kSR));
        float accents[NZONES] = {
            high * (0.55f * midEnv + 0.45f * highEnv) * 0.5f,
            carrier * heartLfo * (0.55f * bassEnv + 0.45f * midEnv) * 0.6f,
            carrier * rmsEnv * 0.7f,
            sub * subEnv * 0.8f,
            feetOsc * subEnv * 0.8f,
        };

        // ── punch-in FX gain for this sample (shared by all zones) ──
        strobePh_ += 9.0f / kSR;
        if (strobePh_ >= 1.0f) strobePh_ -= 1.0f;
        float fxTarget = fxChokeOn ? 0.0f
                       : (fxStrobeOn ? (strobePh_ < 0.5f ? 1.0f : 0.12f) : 1.0f);
        fxG_ = fxCoef * (fxG_ - fxTarget) + fxTarget;
        float tremG = 1.0f;
        if (fxTremD > 0.001f) {
            tremPh_ += dsp::kTwoPi * 5.5f / kSR;
            if (tremPh_ > dsp::kTwoPi) tremPh_ -= dsp::kTwoPi;
            tremG = 1.0f - fxTremD * 0.5f * (1.0f + std::sin(tremPh_));
        }

        // ── mix per zone ──
        for (int z = 0; z < NZONES; z++) {
            int d = maxDelay > 0 ? (z * maxDelay) / 4 : 0;
            float sweep = d > 0 ? sweepDelay_.tap(d) : sweepBase;
            float s = foundation * 0.4f
                    + pulses * pulseW[z] * 0.5f
                    + ((z == HEAD || z == BELLY || z == FEET) ? entA : entB) * 0.35f
                    + sweep * bodyFlow * 0.3f
                    + accents[z] * spread * 0.3f;
            s *= zoneLevel[z];
            s = zoneLP_[z].process(s);

            // gentle running-peak calibration (slow, low max gain, so it can't
            // fight the song's dynamics), then the breath gain rides on top
            zonePeak_[z] = std::max(std::fabs(s), zonePeak_[z] * (1.0f - 1.0f / (30.0f * kSR)));
            float g = zonePeak_[z] > 1e-4f ? (intensity * 0.9f) / zonePeak_[z] : 0.0f;
            g = std::min(g, 6.0f);
            float warmG = warmup_ <= warmHold ? 0.0f
                          : std::min(1.0f, (float)(warmup_ - warmHold) / warmRamp);
            float o = s * g * breathG_ * charGain * swell * warmG;
            // the void ducks only the music chain — pads still strike into it
            o *= 1.0f - voidDepth * voidEnv_;

            // pad play rides on top of the music chain (not through breath/AGC,
            // so the octagon is playable even in total silence)
            {
                float target = 0.0f;
                if (padGateS[z]) {
                    // press-and-hold intensifies: swell to ~1.7x over ~3 s,
                    // so a tap is gentle and a long hold gets heavy
                    float surge = std::min(padT_[z] / 3.0f, 1.0f);
                    target = padVelS[z] * (0.7f + 0.5f * surge);
                }
                float coef = target > padEnv_[z] ? padAtk : padRel;
                padEnv_[z] = coef * (padEnv_[z] - target) + target;
                if (padEnv_[z] > 1e-4f) {
                    float t = padT_[z];
                    if (padGateS[z]) padT_[z] += 1.0f / kSR;
                    float f = padHzS[z];
                    if (patch == PAD_DROP) // dive: 1.6x down to 0.4x over ~2.5 s
                        f *= 1.6f * std::pow(2.0f, -std::min(t, 2.5f) / 1.25f);
                    padPhase_[z] += 2.0f * dsp::kPi * f / kSR;
                    if (padPhase_[z] > 2.0f * dsp::kPi) padPhase_[z] -= 2.0f * dsp::kPi;
                    float ph = padPhase_[z];
                    float y;
                    switch (patch) {
                        case PAD_WHALE: // carrier + 2nd harmonic, slow breathing AM
                            y = (std::sin(ph) + 0.45f * std::sin(2 * ph)) / 1.45f
                                * (0.78f + 0.22f * std::sin(2 * dsp::kPi * 0.22f * t));
                            break;
                        case PAD_QUAKE: { // deep fast tremolo = the shake
                            float trem = 1.0f - 0.7f * (0.5f + 0.5f *
                                std::sin(2 * dsp::kPi * 7.0f * t));
                            y = (std::sin(ph) + 0.4f * std::sin(2 * ph)) / 1.4f * trem;
                            break;
                        }
                        case PAD_HEART: { // lub-dub each 0.9 s
                            float cyc = std::fmod(t, 0.9f);
                            float e = std::exp(-cyc / 0.07f);
                            if (cyc > 0.16f) e += 0.75f * std::exp(-(cyc - 0.16f) / 0.05f);
                            y = std::sin(ph) * std::min(e, 1.0f);
                            break;
                        }
                        case PAD_PURR:
                            y = std::sin(ph) * (0.65f + 0.35f *
                                std::sin(2 * dsp::kPi * 11.0f * t));
                            break;
                        case PAD_DROP:
                            y = (std::sin(ph) + 0.3f * std::sin(2 * ph)) / 1.3f;
                            break;
                        default:
                            y = std::sin(ph);
                    }
                    o += pd.gain * intensity * padEnv_[z] * y;
                }
            }
            o *= fxG_ * tremG;
            o = std::max(-1.0f, std::min(1.0f, o));
            out[z][i] = o;
            if (doScope) vibScope[z][scopeIdx] = o;
            peakAcc[z + 2] = std::max(peakAcc[z + 2], std::fabs(o));
        }
        peakAcc[0] = std::max(peakAcc[0], std::fabs(L));
        peakAcc[1] = std::max(peakAcc[1], std::fabs(R));
        warmup_++;

        if ((i & 1023) == 0) {
            zoneHz[HEAD].store(highFreq <= 200.0f ? highFreq : freq);
            zoneHz[HEART].store(freq);
            zoneHz[BELLY].store(freq);
            zoneHz[ROOT].store(subFreq);
            zoneHz[FEET].store(feetFreq);
        }
    }

    breathNow.store(breathG_);

    // meter ballistics
    for (int m = 0; m < NZONES + 2; m++) {
        float cur = meter[m].load();
        float nx = peakAcc[m];
        meter[m].store(nx > cur ? nx : cur * 0.85f);
    }
}
