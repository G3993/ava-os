// AVA OS — real-time vibroacoustic engine
// Faithful streaming port of the offline 5-module synthesizer:
//   A tonal foundation · B transient pulses · C brainwave entrainment
//   D body sweep · E zone accents
// Zones: 0=HEAD 1=HEART 2=BELLY 3=ROOT 4=FEET
#pragma once
#include <atomic>
#include <cstdint>
#include "dsp.h"
#include "analysis.h"

enum Zone { HEAD = 0, HEART, BELLY, ROOT, FEET, NZONES };

// The bed: 44 ring transducers (HEAD/HEART/BELLY/ROOT) rated 10-80 Hz, and
// one ButtKicker at the centre (FEET) rated 5-200 Hz. Everything the engine
// sends is kept inside the band the hardware can actually turn into motion:
// above 80 Hz a ring coil only heats, and only the centre can carry true sub.
inline float zoneHzMin(int z) { return z == FEET ? 5.0f : 10.0f; }
inline float zoneHzMax(int z) { return z == FEET ? 200.0f : 80.0f; }
// where each zone feels strongest (pads, folds and drones aim here)
inline float zoneSweetLo(int z) { return z == FEET ? 20.0f : 30.0f; }
inline float zoneSweetHi(int z) { return z == FEET ? 80.0f : 70.0f; }

enum Brainwave { DELTA = 0, THETA, ALPHA, BETA, GAMMA };
static const float kBrainwaveHz[5] = {2.0f, 6.0f, 10.0f, 20.0f, 40.0f};
static const char* kBrainwaveName[5] = {"delta", "theta", "alpha", "beta", "gamma"};

struct Params {
    // 0 = BODY: the original five-module synth, every zone a mix of one root
    // 1 = SPLIT: each zone follows its own layer of the song (sub, bass line,
    //     drums, vocal/chord melody, lead/air melody), own pitch, own rhythm
    std::atomic<int>   engineMode{1};
    // Lift: gentle upward expansion of the SPLIT layers — quiet sustained
    // instruments (strings, piano, pads) come up to be felt, loud passages
    // and transients are left alone. 0 = off, 1 = strong
    std::atomic<float> lift{0.55f};
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
    std::atomic<int>   padPatch{0}; // selected voice (index into kPadPatches)
    // voice actually sounding per zone: set by whoever strikes the zone, so a
    // ringing tail keeps its own character when another zone gets a new voice
    std::atomic<int>   padPatchZ[NZONES]{{0}, {0}, {0}, {0}, {0}};
    // punch-in FX bus (MIDI demo mode): momentary while held, act on the
    // whole vibration output — music chain and pads alike
    std::atomic<int>   fxStrobe{0};  // 9 Hz square gate
    std::atomic<int>   fxChoke{0};   // hard mute (20 ms fade)
    std::atomic<float> fxTrem{0};    // tremolo depth 0..0.6 @ 5.5 Hz (mod wheel)
};

// Pad voice library. Every voice is one row: a generic synth in engine.cpp
// evaluates it (harmonic mix, breathing/tremolo AM, pitch envelope, rumble
// noise, hold surge, special patterns). PAD voices are the originals distilled
// from the ADAM stems (the Whale Song's big moment: 26 Hz carrier + 52 Hz
// harmonic on every zone, slow ~0.2 Hz swell, long release). CINEMATIC are
// epic sweeps and hits; DRUM are percussive one-shots meant to be rolled
// head-to-toe by the VFX sequencer.
enum PadCategory { CAT_PAD = 0, CAT_CINEMATIC, CAT_DRUM, NCATS };
// vector glyphs drawn by the UI (main.cpp drawIcon) — one per voice / preset
enum PadIcon { IC_SINE = 0, IC_WHALE, IC_ZIGZAG, IC_HEART, IC_TREMOLO, IC_DROP,
               IC_RISER, IC_HORN, IC_BELL, IC_BURST, IC_BOLT, IC_TENSION,
               IC_KICK, IC_TOM, IC_BOOM, IC_SNARE, IC_ROLL, IC_CLAP,
               IC_UP, IC_DOWN, IC_RIPPLE, IC_BOUNCE, IC_ECG, IC_SWEEP, IC_STAIRS, IC_RISEDROP,
               IC_STORM, IC_SLAM };
enum PadShape { SHAPE_SINE = 0, SHAPE_LUBDUB, SHAPE_ROLL };
enum PadPatch { PAD_PURE = 0, PAD_WHALE, PAD_QUAKE, PAD_HEART, PAD_PURR, PAD_DROP,
                PAD_RISER, PAD_BRAAM, PAD_SWELL, PAD_IMPACT, PAD_THUNDER, PAD_TENSION,
                PAD_KICK, PAD_TOM, PAD_BOOM, PAD_SNARE, PAD_ROLL, PAD_CLAP,
                NPATCHES };
struct PadPatchDef {
    const char* name;
    int cat;
    const char* hint;
    float baseHz;       // <0 = follow the zone's live carrier / tuner
    float atkS, relS;   // amplitude envelope
    float gain;
    float h2;           // 2nd-harmonic mix 0..1
    float amHz, amDepth;    // breathing / tremolo AM (depth 0..1)
    float pitchMul0, pitchMul1, pitchTimeS; // pitch env: ×mul0 → ×mul1 over time
    float noise;        // low-passed rumble noise mix 0..1
    float surge;        // press-and-hold swell amount (0 = none)
    int shape;          // special pattern
    bool allZones;      // strike every zone at once (full-body voice)
    int icon;           // PadIcon
};
static const PadPatchDef kPadPatches[NPATCHES] = {
    // name      cat            hint                       base   atk    rel   gain  h2    amHz  amD   p0    p1   pT    noise surge shape         all
    {"PURE",     CAT_PAD,       "clean tone, tuner pitch",   -1.f, 0.006f, 0.25f, 0.85f, 0.f,  0.f,  0.f,  1.f,  1.f, 0.f,  0.f,  0.5f, SHAPE_SINE,   false, IC_SINE},
    {"WHALE",    CAT_PAD,       "hold to build the surge",   26.f, 2.2f,   1.8f,  1.15f, 0.45f, 0.22f, 0.44f, 1.f, 1.f, 0.f,  0.f,  0.5f, SHAPE_SINE,   true, IC_WHALE},
    {"QUAKE",    CAT_PAD,       "violent 7 Hz shake",        26.f, 0.03f,  0.5f,  1.25f, 0.4f,  7.f,  0.7f,  1.f,  1.f, 0.f,  0.f,  0.5f, SHAPE_SINE,   false, IC_ZIGZAG},
    {"HEART",    CAT_PAD,       "lub-dub every 0.9 s",       38.f, 0.02f,  0.4f,  1.0f,  0.f,   0.f,  0.f,   1.f,  1.f, 0.f,  0.f,  0.5f, SHAPE_LUBDUB, false, IC_HEART},
    {"PURR",     CAT_PAD,       "fast light tremolo",        45.f, 0.08f,  0.35f, 0.7f,  0.f,   11.f, 0.7f,  1.f,  1.f, 0.f,  0.f,  0.5f, SHAPE_SINE,   false, IC_TREMOLO},
    {"DROP",     CAT_PAD,       "pitch dive",                -1.f, 0.04f,  0.9f,  1.0f,  0.3f,  0.f,  0.f,   1.6f, 0.4f, 2.5f, 0.f,  0.5f, SHAPE_SINE,   false, IC_DROP},
    // CINEMATIC — epic sweeps and hits
    {"RISER",    CAT_CINEMATIC, "4 s climb while held",   60.f, 3.5f,   1.2f,  1.1f,  0.3f,  0.5f, 0.15f, 0.5f, 1.f, 4.0f, 0.25f, 0.9f, SHAPE_SINE,  false, IC_RISER},
    {"BRAAM",    CAT_CINEMATIC, "horn, long tail",   36.f, 0.15f,  2.5f,  1.3f,  0.5f,  0.f,  0.f,   1.15f, 1.f, 0.8f, 0.15f, 0.3f, SHAPE_SINE, false, IC_HORN},
    {"SWELL",    CAT_CINEMATIC, "slow bloom, slow fade",     40.f, 2.5f,   3.0f,  1.1f,  0.35f, 0.15f, 0.2f, 1.f,  1.f, 0.f,  0.f,   0.8f, SHAPE_SINE,  false, IC_BELL},
    {"IMPACT",   CAT_CINEMATIC, "hit + rumble tail",         34.f, 0.005f, 2.2f,  1.35f, 0.4f,  0.f,  0.f,   1.8f, 1.f, 0.12f, 0.4f, 0.f,  SHAPE_SINE,  false, IC_BURST},
    {"THUNDER",  CAT_CINEMATIC, "rolling rumble",            28.f, 0.3f,   3.0f,  1.2f,  0.2f,  3.3f, 0.35f, 1.f,  1.f, 0.f,  0.7f,  0.6f, SHAPE_SINE,  false, IC_BOLT},
    {"TENSION",  CAT_CINEMATIC, "rising shimmer",   48.f, 1.5f,   0.6f,  0.9f,  0.f,   9.f,  0.5f,  0.85f, 1.f, 3.0f, 0.f,  0.7f, SHAPE_SINE,  false, IC_TENSION},
    // DRUM — one-shots
    {"KICK",     CAT_DRUM,      "tight punch",               55.f, 0.002f, 0.22f, 1.3f,  0.2f,  0.f,  0.f,   2.4f, 1.f, 0.05f, 0.f,  0.f,  SHAPE_SINE,  false, IC_KICK},
    {"TOM",      CAT_DRUM,      "round thud",                62.f, 0.003f, 0.35f, 1.15f, 0.3f,  0.f,  0.f,   1.5f, 1.f, 0.09f, 0.f,  0.f,  SHAPE_SINE,  false, IC_TOM},
    {"BOOM",     CAT_DRUM,      "deep floor hit",            32.f, 0.004f, 0.9f,  1.35f, 0.25f, 0.f,  0.f,   1.6f, 1.f, 0.08f, 0.2f, 0.f,  SHAPE_SINE,  false, IC_BOOM},
    {"SNARE",    CAT_DRUM,      "noise crack",       78.f, 0.002f, 0.14f, 1.1f,  0.f,   0.f,  0.f,   1.3f, 1.f, 0.03f, 0.6f, 0.f,  SHAPE_SINE,  false, IC_SNARE},
    {"ROLL",     CAT_DRUM,      "retriggers while held",   60.f, 0.01f, 0.2f,  1.0f,  0.2f,  0.f,  0.f,   1.3f, 1.f, 0.03f, 0.f,  0.f,  SHAPE_ROLL,  false, IC_ROLL},
    {"CLAP",     CAT_DRUM,      "short noise slap",          70.f, 0.002f, 0.1f,  1.0f,  0.f,   0.f,  0.f,   1.f,  1.f, 0.f,  0.8f, 0.f,  SHAPE_SINE,  false, IC_CLAP},
};
static const char* kPadCategoryNames[NCATS] = {"PADS", "CINEMATIC", "DRUM"};

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

    // output conditioning per zone: band-limited to what the hardware can move
    dsp::ButterLP zoneLP_[NZONES];
    dsp::Biquad zoneHP_[NZONES];
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
    float padNoiseLP_[NZONES] = {0};   // one-pole rumble noise per zone
    uint32_t padRng_ = 0x9E3779B9u;
    float padT_[NZONES] = {0};      // seconds since strike (patch modulators)
    int padPrevGate_[NZONES] = {0};

    // SPLIT mode state
    dsp::BandPass splitSub_, splitBass_, splitKick_, splitSnare_;
    dsp::EnvFollower splitSubF_, splitBassF_, splitVocF_, splitAirF_;
    dsp::BandPass splitVocBand_, splitAirBand_;
    dsp::Osc oscVoc_, oscAir_, oscKick_, oscSnare_;
    dsp::Smooth vocHzSm_, airHzSm_;
    float kickEnv_ = 0, snareEnv_ = 0;     // drum thump envelopes
    float kickPitch_ = 0;                  // pitch-drop state for the thump
    float splitPeak_[NZONES] = {1e-3f, 1e-3f, 1e-3f, 1e-3f, 1e-3f};
    float vocGate_ = 0, airGate_ = 0;      // smoothed "is this layer present"
    float prevBandOnset_[3] = {0, 0, 0};
    // harmony (second partial → HEART) and low instruments 80-200 Hz (→ ROOT)
    dsp::Osc oscHarm_, oscLow_;
    dsp::Smooth harmHzSm_, lowHzSm_;
    dsp::BandPass splitLowBand_;
    dsp::EnvFollower splitLowF_, splitHarmF_;
    float harmGate_ = 0, lowGate_ = 0;

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
