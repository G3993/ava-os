// AVA OS — real-time vibroacoustic engine
// Faithful streaming port of the offline 5-module synthesizer:
//   A tonal foundation · B transient pulses · C brainwave entrainment
//   D body sweep · E zone accents
// Zones: 0=HEAD 1=HEART 2=BELLY 3=ROOT 4=FEET
#pragma once
#include <atomic>
#include "dsp.h"
#include "analysis.h"

enum Zone { HEAD = 0, HEART, BELLY, ROOT, FEET, NZONES };

enum Brainwave { DELTA = 0, THETA, ALPHA, BETA, GAMMA };
static const float kBrainwaveHz[5] = {2.0f, 6.0f, 10.0f, 20.0f, 40.0f};
static const char* kBrainwaveName[5] = {"delta", "theta", "alpha", "beta", "gamma"};

struct Params {
    std::atomic<float> intensity{0.7f};
    std::atomic<float> grounding{0.5f};
    std::atomic<float> uplift{0.5f};
    std::atomic<float> pulseSync{0.7f};
    std::atomic<int>   brainwave{ALPHA};
    std::atomic<float> bodyFlow{0.4f};
    std::atomic<float> waveWarmth{0.3f};
    std::atomic<float> subDepth{0.4f};
    std::atomic<float> spread{0.6f};
    // Breath: program-dependent expansion — when the song drops below its
    // recent loudness, vibrations drop harder (0 = off, 1 = strong)
    std::atomic<float> breath{0.6f};
    // Dynamics: auto-adapt intensity to the song's character — ambient
    // (few onsets) runs cooler with slow tidal swells; rhythmic runs full
    std::atomic<float> dynamics{0.7f};
    // Void: when the song runs hot, the strongest bass hits carve silence
    // instead of vibration — negative space. 0 = off; higher = voids engage
    // earlier and on more hits (1 = every hard kick is a hole)
    std::atomic<float> voidAmt{0.0f};
    std::atomic<int>   entrainment{1};
    // per-zone levels; <0 means auto (computed from grounding/uplift)
    std::atomic<float> zoneLevel[NZONES]{{-1.f}, {-1.f}, {-1.f}, {-1.f}, {-1.f}};
    std::atomic<float> masterVolume{1.0f};
    std::atomic<int>   monitorVibOnStereo{1};
    // routing (0-based device channel indices)
    std::atomic<int>   musicChanL{0};        // music pair = L,L+1 (ch 1-2 main out); -1 = off
    std::atomic<float> musicGain{1.0f};      // music pair level ×0..1.5
    // surround pair: a second stereo pair carrying the music mix (rear /
    // side speakers). width 0 = mono fill, 1 = full stereo image
    std::atomic<int>   surrChanL{-1};        // surround pair = L,L+1; -1 = off
    std::atomic<float> surrGain{0.8f};
    std::atomic<float> surrWidth{1.0f};
    // bed wiring: HEAD ch9+10 line out (own amp) · BELLY ch5 · ROOT ch6 · FEET (center) ch4 · HEART ch7
    std::atomic<int>   zoneChan[NZONES]{{8}, {6}, {4}, {5}, {3}};
    // optional second send per zone (same signal to a second amp channel); -1 = none
    std::atomic<int>   zoneChan2[NZONES]{{9}, {-1}, {-1}, {-1}, {-1}};
    // per-zone output trim, applied at the interface send (×0..1.5)
    std::atomic<float> zoneTrim[NZONES]{{1.f}, {1.f}, {1.f}, {1.f}, {1.f}};
    // isolate one ring at the interface send: every other zone's output is
    // silenced (music/surround sends untouched); <0 = no solo
    std::atomic<int>   soloZone{-1};
    // transducer thermal guard: cap the ~45 s running RMS leaving each zone
    // channel (0..1 full scale; 0 = off). Voice coils heat with average
    // power, not peaks, so a long hold/drone gets eased down while hits pass.
    std::atomic<float> thermalLimit{0.40f};
    // manual pad play (octagon-as-controller): gate/freq/velocity per zone,
    // synthesized independently of the music chain so pads sound in silence
    std::atomic<int>   padGate[NZONES]{{0}, {0}, {0}, {0}, {0}};
    std::atomic<float> padHz[NZONES]{{82.f}, {55.f}, {55.f}, {27.5f}, {27.5f}};
    std::atomic<float> padVel[NZONES]{{1.f}, {1.f}, {1.f}, {1.f}, {1.f}};
    std::atomic<int>   padPatch{0}; // index into kPadPatches
    // punch-in FX bus (MIDI demo mode): momentary while held, act on the
    // whole vibration output — music chain and pads alike
    std::atomic<int>   fxStrobe{0};  // 9 Hz square gate
    std::atomic<int>   fxChoke{0};   // hard mute (20 ms fade)
    std::atomic<float> fxTrem{0};    // tremolo depth 0..0.6 @ 5.5 Hz (mod wheel)
};

// Pad voice library — characters distilled from the ADAM stems.
// The Whale Song's big moment measured: 26 Hz carrier + 52 Hz harmonic on all
// zones at once, ~5.6x the mean level, slow ~0.2 Hz swell, long build, ~8 s
// release. WHALE/QUAKE encode that; the others cover the calm end.
enum PadPatch { PAD_PURE = 0, PAD_WHALE, PAD_QUAKE, PAD_HEART, PAD_PURR,
                PAD_DROP, NPATCHES };
struct PadPatchDef {
    const char* name;
    float baseHz;   // <0 = follow the zone's live carrier
    float atkS, relS;
    float gain;
};
static const PadPatchDef kPadPatches[NPATCHES] = {
    {"PURE",  -1.0f, 0.006f, 0.25f, 0.85f},
    {"WHALE", 26.0f, 2.2f,   1.8f,  1.15f}, // hold to build the surge
    {"QUAKE", 26.0f, 0.03f,  0.5f,  1.25f}, // violent 7 Hz shake
    {"HEART", 38.0f, 0.02f,  0.4f,  1.0f},  // lub-dub pattern
    {"PURR",  45.0f, 0.08f,  0.35f, 0.7f},  // fast light tremolo
    {"DROP",  -1.0f, 0.04f,  0.9f,  1.0f},  // pitch dive
};

struct Preset {
    const char* name;
    float intensity, grounding, uplift, brainwave, bodyFlow, warmth, subDepth;
    float heartLevel, bellyLevel; // <0 = auto
};
static const Preset kPresets[] = {
    {"Rest",     0.60f, 0.8f, 0.2f, DELTA, 0.3f, 0.2f, 0.5f, -1, -1},
    {"Meditate", 0.65f, 0.6f, 0.4f, THETA, 0.5f, 0.3f, 0.4f, -1, -1},
    {"Focus",    0.70f, 0.4f, 0.6f, ALPHA, 0.4f, 0.4f, 0.3f, -1, -1},
    {"Energize", 0.80f, 0.3f, 0.7f, BETA,  0.6f, 0.6f, 0.3f, -1, -1},
    {"Peak",     0.85f, 0.2f, 0.8f, GAMMA, 0.8f, 0.7f, 0.2f, -1, -1},
    {"Ground",   0.75f, 0.9f, 0.1f, THETA, 0.2f, 0.5f, 0.7f, -1, -1},
    {"Heart",    0.70f, 0.5f, 0.5f, ALPHA, 0.7f, 0.4f, 0.3f, 0.9f, 0.8f},
};
static const int kNumPresets = sizeof(kPresets) / sizeof(kPresets[0]);

class Engine {
public:
    static constexpr float kSR = 48000.0f;
    static constexpr int kScopeLen = 512;

    void init();

    // stereo in (planar) → 5 vibration channels (planar out[5]), n frames.
    // Stereo passthrough is done by the output stage; this fills vibration only.
    void process(const float* inL, const float* inR, float** out, int n);

    Params params;
    StreamAnalyzer analyzer;

    // UI shares
    std::atomic<float> meter[NZONES + 2];   // 0..1 L,R then zones
    std::atomic<float> zoneHz[NZONES];      // live per-zone carrier Hz
    float scope[kScopeLen] = {0};           // mono input waveform (UI reads raw)
    float vibScope[NZONES][kScopeLen] = {{0}}; // per-zone output traces
    std::atomic<int> scopeW{0};

private:
    // envelopes
    dsp::BandPass subBand_, bassBand_, lowMidBand_, midBand_, highBand_;
    dsp::EnvFollower rmsF_, subF_, bassF_, lowMidF_, midF_, highF_;
    dsp::Biquad rmsPre_; // light LP for rms defeating hiss dominance

    // oscillators / lfos
    dsp::Osc oscMid_, oscSub_, oscHigh_, oscFeet_;
    dsp::Osc lfoEnt_, lfoSweep_, lfoHeart_;
    dsp::Smooth freqSm_;

    // onset pulse envelope
    float onsetEnv_ = 0, onsetTarget_ = 0;
    float onsetAtkCoef_ = 0;

    // module D delay
    dsp::DelayLine sweepDelay_;

    // output conditioning per zone
    dsp::ButterLP zoneLP_[NZONES];
    float zonePeak_[NZONES] = {1e-3f, 1e-3f, 1e-3f, 1e-3f, 1e-3f};

    // breath / macro-dynamics
    float loudSq_ = 0;        // ~300 ms mean-square loudness
    float loudMax_ = 1e-6f;   // very slow running max of loudSq_
    float breathG_ = 0;       // smoothed breath gain

    // song-character dynamics
    float onsetRate_ = 0;     // decaying onset counter (τ 8 s)
    float rhythmSm_ = 0.5f;   // smoothed rhythmic-density factor 0..1
    dsp::Osc lfoSwell_;       // slow tidal swell

    // void (negative space on peak hits)
    float hotSm_ = 0;         // smoothed loudness-vs-ceiling (τ 1.5 s)
    float bassMax_ = 1e-6f;   // slow running max of bass envelope
    float voidEnv_ = 0;       // 0..1 duck envelope (1 = full hole)
    long voidHold_ = 0;       // samples left at full duck
    long voidRefrac_ = 0;     // samples until the next void may trigger

    // startup warm-up: mute vibrations while AGC/envelopes settle, then fade in
    long warmup_ = 0;

    // pad play state
    float padEnv_[NZONES] = {0};
    float padPhase_[NZONES] = {0};
    float padT_[NZONES] = {0};      // seconds since strike (patch modulators)
    int padPrevGate_[NZONES] = {0};

    // punch-in FX state
    float fxG_ = 1.0f;       // smoothed strobe/choke gate
    float strobePh_ = 0;     // 0..1 phase of the 9 Hz gate
    float tremPh_ = 0;       // radians

public:
    std::atomic<float> breathNow{0}; // UI: current breath gain 0..1
    std::atomic<float> voidNow{0};   // UI: current void duck 0..1
    // audio features for the shader background (web-host uniform contract)
    std::atomic<float> audioLevel{0}, audioBass{0}, audioMid{0}, audioHigh{0};
    std::atomic<float> audioSubBand{0}, audioLowMidBand{0};

    int scopeDecim_ = 0;
};
