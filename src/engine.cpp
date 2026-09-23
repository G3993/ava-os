#include "engine.h"
#include <algorithm>

// octave-fold a frequency into (lo, hi]: same note, the octave the zone can move
static inline float foldHz(float hz, float lo, float hi) {
    if (hz <= 1.0f) return lo;
    while (hz > hi) hz *= 0.5f;
    while (hz <= lo) hz *= 2.0f;
    return hz;
}

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

    // root glide: short, so a new bass note lands as a note change, not a slide
    freqSm_.design(0.04f, kSR);
    freqSm_.v = 55.0f;
    feetHzSm_.design(0.04f, kSR);
    feetHzSm_.v = 55.0f;
    thumpBand_.design(35, 130, kSR);
    thumpDelay_.init(2048);
    lookM_.init(16384); lookL_.init(16384); lookR_.init(16384);
    synL_.init(16384); synR_.init(16384);
    lowBandM_.design(18, 160, kSR); lowBandL_.design(18, 160, kSR); lowBandR_.design(18, 160, kSR);
    subHarmBand_.design(80, 160, kSR);
    subHarmF_.design(5, 80, kSR, 15.0f);
    subhHzSm_.design(0.03f, kSR); subhHzSm_.v = 41.0f;
    for (int z = 0; z < NZONES; z++) {
        zoneLPR_[z].design(zoneHzMax(z), kSR, 6);
        zoneHPR_[z].highpass(zoneHzMin(z), kSR);
    }
    look_ = 0; anaPos_ = 0; evHead_ = evTail_ = 0;
    for (float& f : f0Hist_) f = 55.0f;

    onsetAtkCoef_ = std::exp(-1.0f / (0.008f * kSR));

    sweepDelay_.init(32768); // > 300 ms @ 48k

    for (int z = 0; z < NZONES; z++) {
        zoneLP_[z].design(zoneHzMax(z), kSR, 6);
        zoneHP_[z].highpass(zoneHzMin(z), kSR);
    }

    // SPLIT mode: real low bands pass straight to the body; the layers that
    // live above the felt range are tracked and folded down
    splitSub_.design(8, 50, kSR);      // true sub, only the ButtKicker can move it
    splitBass_.design(40, 80, kSR);    // the bass line inside the ring band
    splitKick_.design(40, 80, kSR);
    splitSnare_.design(150, 400, kSR);
    splitVocBand_.design(160, 900, kSR);
    splitAirBand_.design(900, 6000, kSR);
    splitSubF_.design(5, 60, kSR, 15.0f);
    splitBassF_.design(5, 60, kSR, 15.0f);
    splitVocF_.design(6, 90, kSR, 15.0f);
    splitAirF_.design(4, 70, kSR, 15.0f);
    vocHzSm_.design(0.03f, kSR); vocHzSm_.v = 55.0f;
    airHzSm_.design(0.03f, kSR); airHzSm_.v = 70.0f;
    harmHzSm_.design(0.03f, kSR); harmHzSm_.v = 55.0f;
    lowHzSm_.design(0.03f, kSR); lowHzSm_.v = 27.5f;
    splitLowBand_.design(80, 200, kSR);          // cello, low piano, low synth
    splitLowF_.design(12, 220, kSR, 15.0f);      // slower: these are sustained
    splitHarmF_.design(25, 350, kSR, 15.0f);     // chords/strings bloom, not snap
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
    const PadPatchDef* pdz[NZONES];
    float padAtkZ[NZONES], padRelZ[NZONES];
    for (int z = 0; z < NZONES; z++) {
        int patch = std::min(std::max(params.padPatchZ[z].load(), 0), NPATCHES - 1);
        pdz[z] = &kPadPatches[patch];
        padAtkZ[z] = std::exp(-1.0f / (pdz[z]->atkS * kSR));
        padRelZ[z] = std::exp(-1.0f / (pdz[z]->relS * kSR));
        padGateS[z] = params.padGate[z].load();
        padHzS[z] = params.padHz[z].load();
        padVelS[z] = params.padVel[z].load();
        if (padGateS[z] && !padPrevGate_[z]) padT_[z] = 0; // new strike
        padPrevGate_[z] = padGateS[z];
    }

    // no sound at open: hold silent 0.6 s while followers settle, fade in 0.9 s
    const long warmHold = (long)(0.6f * kSR);
    const long warmRamp = (long)(0.9f * kSR);

    // punch-in FX (momentary): strobe gates at 9 Hz, choke mutes; both ride
    // one smoothed gain (~8 ms) so edges thump instead of clicking
    const int fxStrobeOn = params.fxStrobe.load();
    const int fxChokeOn = params.fxChoke.load();
    const int  mode = params.engineMode.load();
    const bool synthMode  = mode == 0;
    const bool splitMode  = mode == 1 || mode == 4;   // BODY builds on the SPLIT layers
    const bool stereoMode = mode == 3 && n <= kMaxBlock;
    stereoOut_ = stereoMode;
    // bass drop: sub quiet (< 0.25 of its max) for 1.5 s, then a hit that
    // brings it above 0.75 — the drop. At most one effect every 8 s.
    const long dropRefracLen = (long)(8.0f * kSR);
    // Lift: e^(1-0.6·lift) — a soft upward expander on normalized envelopes.
    // Transient layers (kick, snare) never pass through it, so punch stays.
    const float liftExp = 1.0f - 0.6f * std::min(1.0f, std::max(0.0f, params.lift.load()));
    auto lifted = [liftExp](float e) { return e > 1e-4f ? std::pow(e, liftExp) : 0.0f; };
    const float fxTremD = params.fxTrem.load();
    const float fxCoef = std::exp(-1.0f / (0.008f * kSR));

    // look-ahead for this block; detector latencies to back-date events by
    look_ = std::min(kLookMax, std::max(0, (int)(params.syncLookaheadMs.load() * 0.001f * kSR)));
    const int look = look_;
    const long dThump = (long)(0.016f * kSR);   // band filter + attack, plus the
                                                 // 42 Hz thump's own rise through the 80 Hz lowpass
    const long dSnare = (long)(0.025f * kSR);   // FFT window centre + hop
    const long dPitch = (long)(0.060f * kSR);   // YIN window centre + persistence
    // pass-through low bands (sub / bass / low instruments) reach the body
    // through 4th-order bandpasses and the 6th-order zone lowpass: ~26 ms of
    // group delay at 20-60 Hz. With look-ahead they read the input that much
    // earlier, so the felt bass lands with the heard bass.
    const int leadLow = std::min(look, (int)(0.026f * kSR));
    // MONO/STEREO paths are shorter filters: 18 ms for the rings (their 80 Hz
    // zone lowpass adds ~9 ms over the feet's 200 Hz one), 9 ms for the feet
    const int leadRing = std::min(look, (int)(0.018f * kSR));
    const int leadFeet = std::min(look, (int)(0.009f * kSR));
    // the envelope followers behind the BODY carriers have a 10 ms attack
    const int leadEnv = std::min(look, (int)(0.010f * kSR));

    // low-end transient detector: 2 ms attack / 50 ms release envelope of the
    // 35-130 Hz band, compared with its own recent peak (150 ms peak-hold,
    // read 30 ms back). A kick or bass pluck is a >1.7x jump (a kick over a
    // held bass note only lifts the summed envelope ~2x; the ripple of a bare
    // 35 Hz tone is ~1.3x). The peak-hold means the kick's tail beating
    // against the bass cannot re-fire; a held note never fires; 110 ms refractory
    const float thumpAtk = std::exp(-1.0f / (0.002f * kSR));
    const float thumpRel = std::exp(-1.0f / (0.050f * kSR));
    const float thumpHoldRel = std::exp(-1.0f / (0.150f * kSR));
    const float thumpMaxDecay = 1.0f - 1.0f / (20.0f * kSR);
    const int   thumpLook = (int)(0.030f * kSR);
    const long  thumpRefrac = (long)(0.110f * kSR);

    for (int i = 0; i < n; i++) {
        float L = inL[i], R = inR[i];
        // ── look-ahead: the analysis hears now; the synth hears `look` later ──
        float monoA = 0.5f * (L + R);
        lookM_.push(monoA);
        float mono = look > 0 ? lookM_.tap(look) : monoA;
        const float monoLow = look > 0 ? lookM_.tap(look - leadLow) : monoA;
        const float monoRing = look > 0 ? lookM_.tap(look - leadRing) : monoA;
        const float monoFeet = look > 0 ? lookM_.tap(look - leadFeet) : monoA;
        synL_.push(L); synR_.push(R);
        const float lowL = look > 0 ? synL_.tap(look - leadRing) : L;
        const float lowR = look > 0 ? synR_.tap(look - leadRing) : R;
        const float monoEnv = look > 0 ? lookM_.tap(look - leadEnv) : monoA;
        anaPos_++;
        const long synthPos = anaPos_ - look;

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

        // ── analysis (on the undelayed signal) ──
        // spectral-flux onsets feed tempo and rhythmic density; the felt
        // pulses come from the low-end transient below (sample-accurate).
        // Each detected event is queued at its back-dated onset in the
        // synth's clock; with look-ahead the synth reaches it exactly then.
        float onset = analyzer.pushSample(monoA);
        auto queueEv = [this](long at, float v, int kind) {
            int next = (evTail_ + 1) % kEvQ;
            if (next == evHead_) return;             // queue full: drop
            evQ_[evTail_] = {at, v, kind};
            evTail_ = next;
        };
        {
            float ta = std::fabs(thumpBand_.process(monoA));
            thumpFast_ = (ta > thumpFast_ ? thumpAtk : thumpRel) * (thumpFast_ - ta) + ta;
            thumpHold_ = std::max(thumpFast_, thumpHold_ * thumpHoldRel);
            float before = thumpDelay_.tap(thumpLook);
            thumpDelay_.push(thumpHold_);
            thumpMax_ = std::max(thumpFast_, thumpMax_ * thumpMaxDecay);
            thumpSince_++;
            if (thumpSince_ > thumpRefrac && thumpFast_ > 0.12f * thumpMax_
                && thumpFast_ > 1.7f * std::max(before, 0.02f * thumpMax_)) {
                queueEv(anaPos_ - dThump, std::min(1.0f, thumpFast_ / thumpMax_), 0);
                thumpSince_ = 0;
                thumpCount.fetch_add(1, std::memory_order_relaxed);
            }
            // snare band (160-900 Hz) flux, from the FFT: back-dated further
            float snareOn = analyzer.out.bandOnset[1].load();
            if (snareOn > 0.6f && snareOn > prevSnareOn_ + 0.2f) queueEv(anaPos_ - dSnare, snareOn * 0.8f, 1);
            prevSnareOn_ = snareOn;
            // the bass note as published, every 64 samples, so the synth can
            // read the note that belongs to the sample it is rendering
            if ((anaPos_ & 63) == 0) f0Hist_[(anaPos_ >> 6) & 511] = analyzer.out.f0Hz.load();
        }
        // ── synth side: events whose onset the synth clock has reached ──
        float thump = 0.0f;
        while (evHead_ != evTail_ && evQ_[evHead_].at <= synthPos) {
            const Ev& e = evQ_[evHead_];
            if (e.kind == 0) thump = std::max(thump, e.v);
            else snareEnv_ = std::max(snareEnv_, e.v);
            evHead_ = (evHead_ + 1) % kEvQ;
        }
        if (thump > 0) onsetTarget_ = std::min(1.0f, onsetTarget_ + thump);

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
        // breath pacer: raised-sine swell at a breathing rate on top
        {
            float ph = params.pacerHz.load();
            if (ph > 0.001f) {
                float d = std::min(0.9f, std::max(0.0f, params.pacerDepth.load()));
                float b = 0.5f + 0.5f * lfoPacer_.tick(ph, kSR);   // 0 = out-breath, 1 = in-breath
                swell *= (1.0f - d) + d * (0.35f + 0.65f * b);
            }
        }

        // onset pulse env: fast attack toward target, tempo-scaled decay
        onsetTarget_ *= onsetDecayCoef;
        onsetEnv_ = onsetAtkCoef_ * (onsetEnv_ - onsetTarget_) + onsetTarget_;

        // envelopes (normalized by running max, mirroring offline normalize)
        float rmsEnv    = rmsF_.process(rmsPre_.process(monoEnv));
        float subEnv    = subF_.process(subBand_.process(monoEnv));
        float bassEnv   = bassF_.process(bassBand_.process(monoEnv));
        float lowMidEnv = lowMidF_.process(lowMidBand_.process(monoEnv));
        float midEnv    = midF_.process(midBand_.process(monoEnv));
        float highEnv   = highF_.process(highBand_.process(monoEnv));
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
            // the thump fires within ~2 ms of the hit, before the 10 ms bass
            // follower has seen it: arm, and judge the hit 12 ms later
            if (voidAmt > 0.01f && thump > 0 && voidRefrac_ == 0 && voidArm_ == 0)
                voidArm_ = (long)(0.012f * kSR);
            if (voidArm_ > 0 && --voidArm_ == 0
                && hotSm_ > hotTh && bassNow > bassTh * bassMax_) {
                voidHold_ = voidHoldLen;
                voidRefrac_ = voidRefracLen;
            }
            float vt = voidHold_ > 0 ? 1.0f : 0.0f;
            float vc = vt > voidEnv_ ? voidAtkCoef : voidRelCoef;
            voidEnv_ = vc * (voidEnv_ - vt) + vt;
        }

        // ── root: the lowest pitch of the music, doubled ──
        // f0 is the measured fundamental of the bass (analysis.cpp). The bed
        // is tuned to 2·f0; the rings (10-80 Hz) take it octave-folded into
        // 40-80 Hz so it is always the same note in the octave they can move,
        // and the feet (ButtKicker, 5-200 Hz) carry the fundamental itself.
        float f0;
        if (look >= dPitch) {
            // the note the analyser had published `dPitch` after this sample:
            // i.e. the note that actually starts here
            long pubAt = synthPos + dPitch;
            f0 = f0Hist_[(pubAt >> 6) & 511];
        } else {
            f0 = analyzer.out.f0Hz.load();
        }
        const float rootRaw = 2.0f * f0;
        float freq = freqSm_.tick(foldHz(rootRaw, 40.0f, 80.0f));
        float subFreq = freq * 0.5f;
        float highFreq = freq * 1.5f;
        float feetFreq = feetHzSm_.tick(foldHz(f0, 20.0f, 80.0f));

        // oscillators
        float carrier = oscMid_.tick(freq, kSR);
        float sub = oscSub_.tick(subFreq, kSR);
        float high = oscHigh_.tick(highFreq <= zoneHzMax(HEAD) ? highFreq : freq, kSR);
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

        // ── SPLIT: five layers of the song, one per zone ──
        float split[NZONES] = {0, 0, 0, 0, 0};
        // real bass, felt directly (8-50 sub, 40-80 bass line): one filter
        // step per sample, shared by SPLIT, BODY, MONO and STEREO
        const float subSig = splitSub_.process(monoLow);
        const float bassSig = splitBass_.process(monoLow);
        if (splitMode) {
            float subE = splitSubF_.process(subSig);
            float bassE = splitBassF_.process(bassSig);
            // drums: the kick band's transient, and the snare band's, each
            // shaped into a thump with a pitch drop so hits read as hits
            if (thump > 0) { kickEnv_ = std::max(kickEnv_, thump); kickPitch_ = 1.0f; }
            kickEnv_ *= 1.0f - 1.0f / (0.09f * kSR);
            snareEnv_ *= 1.0f - 1.0f / (0.05f * kSR);
            kickPitch_ *= 1.0f - 1.0f / (0.04f * kSR);
            // kick thump: 42 Hz with a drop from 72 Hz, all inside the ring band
            float kick = oscKick_.tick(42.0f * (1.0f + 0.7f * kickPitch_), kSR) * kickEnv_;
            float snare = oscSnare_.tick(72.0f, kSR) * snareEnv_;
            // every tonal layer plays the root (lowest pitch x2, folded to the
            // ring octave): one true note across the body, each zone with its
            // own envelope. No per-band partial picking.
            // ── HEAD = the voice. Its weight follows the vocal band's envelope,
            // boosted by the 0.9-6 kHz presence that a voice has and a piano
            // chord mostly doesn't ──
            float vocHz = vocHzSm_.tick(freq);
            float vocE = lifted(splitVocF_.process(splitVocBand_.process(mono)));
            float airE = lifted(splitAirF_.process(splitAirBand_.process(mono)));
            float vocPres = analyzer.out.bandPeakMag[1].load();
            vocGate_ += ((vocPres > 0.18f ? 1.0f : 0.0f) - vocGate_) * (1.0f / (0.08f * kSR));
            float voice = (std::sin(oscVoc_.phase) + 0.35f * oscVoc_.harmonic(2)) / 1.35f
                          * vocE * (0.55f + 0.75f * airE) * vocGate_;
            oscVoc_.tick(vocHz, kSR);
            // ── HEART = the harmony. Second partial of the same band (a different
            // note from the voice: the chord, strings, piano under it), slow
            // bloom so sustained instruments swell rather than tick ──
            float harmHz = harmHzSm_.tick(freq);
            float harmE = lifted(splitHarmF_.process(splitVocBand_.process(mono)));
            float harmPres = analyzer.out.bandPeak2Mag[1].load();
            harmGate_ += ((harmPres > 0.14f ? 1.0f : 0.0f) - harmGate_) * (1.0f / (0.15f * kSR));
            float harm = (std::sin(oscHarm_.phase) + 0.25f * oscHarm_.harmonic(2)) / 1.25f * harmE * harmGate_;
            oscHarm_.tick(harmHz, kSR);
            // ── ROOT also carries the low instruments (80-200 Hz: cello, low
            // piano, low synth) that the rings cannot reproduce directly:
            // folded down, lifted, so a quiet low line is still felt ──
            float lowHz = lowHzSm_.tick(freq * 0.5f); // an octave under the rings' root
            float lowE = lifted(splitLowF_.process(splitLowBand_.process(monoLow)));
            float lowPres = analyzer.out.bandPeakMag[0].load();
            lowGate_ += ((lowPres > 0.15f ? 1.0f : 0.0f) - lowGate_) * (1.0f / (0.12f * kSR));
            float low = std::sin(oscLow_.phase) * lowE * lowGate_;
            oscLow_.tick(lowHz, kSR);
            // sub and bass line pass straight through, gently lifted at the send
            float subLift = 0.7f + 0.6f * lifted(subE);
            float bassLift = 0.7f + 0.6f * lifted(bassE);
            split[FEET]  = subSig * 3.2f * subLift + bassSig * 0.4f;
            split[ROOT]  = bassSig * 2.6f * bassLift + low * 1.3f + subSig * 0.5f;
            split[BELLY] = kick * 1.5f + bassSig * 0.5f * bassE;
            split[HEART] = harm * 1.5f + kick * 0.3f;
            split[HEAD]  = voice * 1.8f + snare * 0.7f;
            if ((i & 1023) == 0) {
                zoneHz[HEART].store(harmHz);
                zoneHz[HEAD].store(vocHz);
                zoneHz[ROOT].store(lowHz);
            }
        }

        // ── MONO / STEREO / BODY: the song itself, felt ──
        float mix[NZONES] = {0, 0, 0, 0, 0}, mixR[NZONES] = {0, 0, 0, 0, 0};
        if (mode >= 2) {
            // octave-down copy of the 80-160 Hz bass (a note the rings cannot
            // move) on the tracked root, so a high bass line is still felt
            float subhE = subHarmF_.process(subHarmBand_.process(monoLow));
            float subhHz = subhHzSm_.tick(foldHz(f0, 30.0f, 60.0f));
            float subh = oscSubh_.tick(subhHz, kSR) * subhE;
            if (mode == 2) {
                // MONO: 18-160 Hz to every zone (rings band-limit to 80 below),
                // the 18-80 bass exaggerated x2 = the low end itself plus once more
                float lowRing = lowBandM_.process(monoRing);
                float lowFeet = lowBandL_.process(monoFeet);      // own filter state for the feet tap
                float bassX = bassSig + subSig;                    // 8-80 Hz, once more = x2
                for (int z = 0; z < NZONES; z++) mix[z] = (z == FEET ? lowFeet : lowRing) + bassX + subh * 0.9f;
            } else if (mode == 3) {
                // STEREO: left / right low end with the low-end width doubled
                // (bass is nearly mono in most mixes; x2 side makes a pan felt)
                float mid = 0.5f * (lowL + lowR), side = 0.5f * (lowL - lowR);
                float lL = lowBandL_.process(mid + 2.0f * side);
                float lR = lowBandR_.process(mid - 2.0f * side);
                float bassX = bassSig + subSig;
                float lowFeet = lowBandM_.process(monoFeet);
                for (int z = 0; z < NZONES; z++) {
                    if (z == FEET) { mix[z] = mixR[z] = lowFeet + bassX + subh * 0.9f; }
                    else { mix[z] = lL + 0.5f * bassX + subh * 0.7f; mixR[z] = lR + 0.5f * bassX + subh * 0.7f; }
                }
            } else {
                // BODY: 3 channels. Centre = the voice on HEAD+HEART, mains = the
                // bass line + drums on BELLY+ROOT, LFE = the sub on FEET (+10 dB,
                // bass-managed: only what the ButtKicker can move)
                float centre = split[HEAD] * 0.9f;                       // voice + snare
                float mains  = split[ROOT] * 0.8f + split[BELLY] * 0.8f; // bass line, low instruments, kick
                float lfe    = subSig * 3.16f * 3.2f + subh * 0.8f;
                mix[HEAD] = centre; mix[HEART] = centre;
                mix[BELLY] = mains; mix[ROOT] = mains;
                mix[FEET] = lfe;
            }
        }
        // bass-drop detector (all modes track it; BODY fires the effect)
        {
            // raw (un-normalized) sub level against a 60 s peak: a lull is the
            // sub under 15% of that peak or under -40 dBFS; a drop is a hit,
            // after 1.5 s of lull, that brings it back over 60% of the peak
            // (or 6x the lull level). Judged 40 ms after the hit, when the
            // kick's own sub energy and the 10 ms follower are both in.
            const float subRaw = subF_.env;
            subAbsMax_ = std::max(subRaw, subAbsMax_ * (1.0f - 1.0f / (60.0f * kSR)));
            const bool subQuiet = subRaw < std::max(0.01f, 0.15f * subAbsMax_);
            if (dropRefrac_ > 0) dropRefrac_--;
            if (thump > 0 && subQuietS_ > 1.5f && dropRefrac_ == 0 && dropArm_ == 0)
                dropArm_ = (long)(0.040f * kSR);
            if (dropArm_ > 0 && --dropArm_ == 0) {
                if (subRaw > std::max(std::max(0.4f * subAbsMax_, 6.0f * subQuietLvl_), 0.02f)) {
                    dropRefrac_ = dropRefracLen;
                    subQuietS_ = 0;
                    dropFlash.store(1.0f);
                    if (mode == 4) { dropT_ = 0; for (int z = 0; z < NZONES; z++) dropPh_[z] = 0; }
                }
            }
            if (subQuiet) {
                subQuietS_ += 1.0f / kSR;
                subQuietLvl_ += (subRaw - subQuietLvl_) * (1.0f / (0.5f * kSR));
            } else if (dropArm_ == 0 && subRaw > 0.3f * subAbsMax_) {
                subQuietS_ = 0;
            }
            if (dropT_ >= 0) { dropT_ += 1.0f / kSR; if (dropT_ > 1.6f) dropT_ = -1; }
        }

        // ── mix per zone ──
        for (int z = 0; z < NZONES; z++) {
            int d = maxDelay > 0 ? (z * maxDelay) / 4 : 0;
            float sweep = d > 0 ? sweepDelay_.tap(d) : sweepBase;
            float s, sR = 0;
            if (mode >= 2) {
                s = mix[z]; sR = mixR[z];
            } else if (splitMode) {
                // the layer itself, plus a little of the shared pulse so the
                // whole body still agrees on the downbeat
                s = split[z] + pulses * pulseW[z] * 0.15f + sweep * bodyFlow * 0.12f;
            } else {
                s = foundation * 0.4f
                  + pulses * pulseW[z] * 0.5f
                  + ((z == HEAD || z == BELLY || z == FEET) ? entA : entB) * 0.35f
                  + sweep * bodyFlow * 0.3f
                  + accents[z] * spread * 0.3f;
            }
            s *= zoneLevel[z];
            s = zoneLP_[z].process(zoneHP_[z].process(s));
            if (stereoMode) { sR *= zoneLevel[z]; sR = zoneLPR_[z].process(zoneHPR_[z].process(sR)); }

            // gentle running-peak calibration (slow, low max gain, so it can't
            // fight the song's dynamics), then the breath gain rides on top.
            // In STEREO both sides share one gain so the image is preserved.
            float pk = stereoMode ? std::max(std::fabs(s), std::fabs(sR)) : std::fabs(s);
            zonePeak_[z] = std::max(pk, zonePeak_[z] * (1.0f - 1.0f / (30.0f * kSR)));
            float g = zonePeak_[z] > 1e-4f ? (intensity * 0.9f) / zonePeak_[z] : 0.0f;
            g = std::min(g, 6.0f);
            float warmG = warmup_ <= warmHold ? 0.0f
                          : std::min(1.0f, (float)(warmup_ - warmHold) / warmRamp);
            const float chainG = g * breathG_ * charGain * swell * warmG * (1.0f - voidDepth * voidEnv_);
            float o = s * chainG;
            float oR = sR * chainG;
            // the void ducks only the music chain — pads still strike into it
            // (folded into chainG above)
            // BODY drop effect: a head-to-toe roll, 110 ms per zone, pitch 70 → 30 Hz
            if (dropT_ >= 0) {
                float t = dropT_ - z * 0.11f;
                if (t > 0) {
                    float env = std::min(t / 0.03f, 1.0f) * std::exp(-std::max(0.0f, t - 0.05f) / 0.35f);
                    float f = 30.0f + 40.0f * std::exp(-t * 6.0f);
                    dropPh_[z] += dsp::kTwoPi * f / kSR;
                    if (dropPh_[z] > dsp::kTwoPi) dropPh_[z] -= dsp::kTwoPi;
                    float y = std::sin(dropPh_[z]) * env * intensity * 1.1f;
                    o += y; oR += y;
                }
            }

            // pad play rides on top of the music chain (not through breath/AGC,
            // so the octagon is playable even in total silence)
            {
                const PadPatchDef& pd = *pdz[z];
                const float padAtk = padAtkZ[z], padRel = padRelZ[z];
                float target = 0.0f;
                if (padGateS[z]) {
                    // press-and-hold intensifies: swell to ~1.7x over ~3 s,
                    // so a tap is gentle and a long hold gets heavy
                    float surge = std::min(padT_[z] / 3.0f, 1.0f);
                    target = padVelS[z] * (0.7f + pd.surge * surge);
                }
                float coef = target > padEnv_[z] ? padAtk : padRel;
                padEnv_[z] = coef * (padEnv_[z] - target) + target;
                if (padEnv_[z] > 1e-4f) {
                    float t = padT_[z];
                    if (padGateS[z]) padT_[z] += 1.0f / kSR;
                    // pitch envelope: ×mul0 → ×mul1 over pitchTimeS (exponential)
                    float f = padHzS[z];
                    if (pd.pitchTimeS > 0.0f) {
                        float k = std::min(t / pd.pitchTimeS, 1.0f);
                        f *= pd.pitchMul0 * std::pow(pd.pitchMul1 / pd.pitchMul0, k);
                    }
                    padPhase_[z] += 2.0f * dsp::kPi * f / kSR;
                    if (padPhase_[z] > 2.0f * dsp::kPi) padPhase_[z] -= 2.0f * dsp::kPi;
                    float ph = padPhase_[z];
                    // harmonic mix
                    float y = (std::sin(ph) + pd.h2 * std::sin(2 * ph)) / (1.0f + pd.h2);
                    // rumble: white noise through a one-pole ~90 Hz lowpass
                    if (pd.noise > 0.0f) {
                        padRng_ ^= padRng_ << 13; padRng_ ^= padRng_ >> 17; padRng_ ^= padRng_ << 5;
                        float wn = ((padRng_ >> 8) * (1.0f / 8388608.0f)) - 1.0f;
                        padNoiseLP_[z] += 0.0118f * (wn - padNoiseLP_[z]);
                        y = y * (1.0f - 0.6f * pd.noise) + pd.noise * 6.0f * padNoiseLP_[z];
                    }
                    // breathing / tremolo AM
                    if (pd.amHz > 0.0f)
                        y *= 1.0f - pd.amDepth * (0.5f + 0.5f * std::sin(2 * dsp::kPi * pd.amHz * t));
                    // special patterns
                    if (pd.shape == SHAPE_LUBDUB) { // lub-dub each 0.9 s
                        float cyc = std::fmod(t, 0.9f);
                        float e = std::exp(-cyc / 0.07f);
                        if (cyc > 0.16f) e += 0.75f * std::exp(-(cyc - 0.16f) / 0.05f);
                        y *= std::min(e, 1.0f);
                    } else if (pd.shape == SHAPE_ROLL) { // 11 Hz retrigger
                        float cyc = std::fmod(t, 1.0f / 11.0f);
                        y *= std::exp(-cyc / 0.04f);
                    }
                    float py = pd.gain * intensity * padEnv_[z] * y;
                    o += py; oR += py;
                }
            }
            o *= fxG_ * tremG;
            o = std::max(-1.0f, std::min(1.0f, o));
            out[z][i] = o;
            if (stereoMode) { oR *= fxG_ * tremG; vibR_[z][i] = std::max(-1.0f, std::min(1.0f, oR)); }
            if (doScope) vibScope[z][scopeIdx] = o;
            peakAcc[z + 2] = std::max(peakAcc[z + 2], std::fabs(o));
        }
        peakAcc[0] = std::max(peakAcc[0], std::fabs(L));
        peakAcc[1] = std::max(peakAcc[1], std::fabs(R));
        warmup_++;

        if ((i & 1023) == 0) {
            if (mode >= 2) {
                float sh = subhHzSm_.v;
                for (int z = 0; z < NZONES; z++) zoneHz[z].store(z == FEET ? feetFreq : sh);
            } else {
                if (!splitMode) {
                    zoneHz[HEAD].store(highFreq <= zoneHzMax(HEAD) ? highFreq : freq);
                    zoneHz[HEART].store(freq);
                }
                zoneHz[BELLY].store(freq);
                if (!splitMode) zoneHz[ROOT].store(subFreq);
                zoneHz[FEET].store(feetFreq);
            }
        }
    }

    breathNow.store(breathG_);
    { float d = dropFlash.load(); if (d > 0) dropFlash.store(d * 0.97f); }

    // meter ballistics
    for (int m = 0; m < NZONES + 2; m++) {
        float cur = meter[m].load();
        float nx = peakAcc[m];
        meter[m].store(nx > cur ? nx : cur * 0.85f);
    }
}

void Engine::delayMusic(const float* L, const float* R, float* oL, float* oR, int n) {
    const int look = look_;
    for (int i = 0; i < n; i++) {
        lookL_.push(L[i]); lookR_.push(R[i]);
        oL[i] = look > 0 ? lookL_.tap(look) : L[i];
        oR[i] = look > 0 ? lookR_.tap(look) : R[i];
    }
}
