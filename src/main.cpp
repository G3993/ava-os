// AVA OS — live system-audio vibroacoustic engine
// UI closely mirrors the SoundTemple octagon control surface:
// left octagon visualization, right glass panel with preset tabs,
// master waveform, and five zone slider columns.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>
#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "engine.h"
#include "capture_tap.h"
#include "audio_out.h"
#ifdef __APPLE__
#include "audio_in.h"
#endif
#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#endif
#include "shaderhost.h"
#ifdef __APPLE__
#include "midi_in.h"
#include "midi_out.h"
#endif

// ─────────────────────────────────────────────────────────────────────────
// Self-test (no UI): prove the engine synthesizes and the tap gets signal
// ─────────────────────────────────────────────────────────────────────────
static int runSelfTest();

#ifndef TEMPLE_SELFTEST_ONLY
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

// ── app state ──
static Engine gEngine;
static SystemTap gTap;
static OutputUnit gOut;
static StereoRing gRing;
#ifdef __APPLE__
static StereoRing gInRing;   // live input from the interface (guitar, mic…)
static InputUnit gIn;
static bool gInOn = true;   // on by default: input 1 (the guitar jack) feeds the engine
#endif
static std::vector<OutDevice> gDevices;
static int gSelDevice = -1;
static int gActiveTab = 0;
static int gMode = 0;        // right card: 0 Audio · 1 Visual · 2 Artifact
static int gArtifactTab = 0; // Artifact: 0 Sounds · 1 Tuner · 2 MIDI
static float gZoneSlider[NZONES] = {0.65f, 0.6f, 0.8f, 0.75f, 0.75f};
static float gEdgeFade = 0.0f; // black vignette on the shader/projector output
static float gMasterVol = 1.0f;
static bool gPlaying = false;
static ShaderHost gShaders;
static GLFWwindow* gExtWin = nullptr;
static double gShaderT0 = 0;
// single "Balance" control drives grounding/uplift (all presets are complementary)
static std::atomic<float> gBalance{0.5f};
static ImFont* gFontSmall = nullptr;

// inset a convex polygon by a fixed pixel margin (equal padding on every edge)
static void insetConvexPoly(const ImVec2* p, int n, float g, ImVec2* out) {
    ImVec2 c(0, 0);
    for (int i = 0; i < n; i++) { c.x += p[i].x; c.y += p[i].y; }
    c.x /= n; c.y /= n;
    struct L { float a, b, d; };
    static thread_local std::vector<L> ls;
    ls.resize(n);
    for (int i = 0; i < n; i++) {
        ImVec2 A = p[i], B = p[(i + 1) % n];
        float dx = B.x - A.x, dy = B.y - A.y;
        float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-4f) len = 1e-4f;
        float nx = dy / len, ny = -dx / len;                 // edge normal
        if ((c.x - A.x) * nx + (c.y - A.y) * ny < 0) { nx = -nx; ny = -ny; } // inward
        ls[i] = {nx, ny, nx * (A.x + nx * g) + ny * (A.y + ny * g)};
    }
    for (int i = 0; i < n; i++) {
        const L& l1 = ls[(i + n - 1) % n];
        const L& l2 = ls[i];
        float det = l1.a * l2.b - l2.a * l1.b;
        if (std::fabs(det) < 1e-6f) { out[i] = p[i]; continue; }
        out[i].x = (l1.d * l2.b - l2.d * l1.b) / det;
        out[i].y = (l1.a * l2.d - l2.a * l1.d) / det;
    }
}

static const char* kZoneNames[NZONES] = {"HEAD", "HEART", "BELLY", "BUTT", "FEET"};
static const char* kTabNames[5] = {"Calm", "Heal", "Breath", "Energy", "Creative"};
static const int kTabPreset[5] = {7, 8, 1, 3, 4}; // Calm, Heal, Meditate, Energize, Peak
static const char* kKeyNames[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

static ImU32 W(float a) { return IM_COL32(255, 255, 255, (int)(a * 255)); }

static void applyPresetTuning(int t);   // defined after the tunings table
static void applyPreset(int p) {
    const Preset& pr = kPresets[p];
    gEngine.params.intensity.store(pr.intensity);
    gEngine.params.grounding.store(pr.grounding);
    gEngine.params.uplift.store(pr.uplift);
    gEngine.params.brainwave.store((int)pr.brainwave);
    gEngine.params.bodyFlow.store(pr.bodyFlow);
    gEngine.params.waveWarmth.store(pr.warmth);
    gEngine.params.subDepth.store(pr.subDepth);
    gBalance.store(pr.uplift);
    float auto5[NZONES] = {0.5f + pr.uplift * 0.5f, 0.5f + pr.uplift * 0.3f, 0.8f,
                           0.5f + pr.grounding * 0.5f, 0.5f + pr.grounding * 0.5f};
    if (pr.heartLevel >= 0) auto5[HEART] = pr.heartLevel;
    if (pr.bellyLevel >= 0) auto5[BELLY] = pr.bellyLevel;
    for (int z = 0; z < NZONES; z++) {
        gZoneSlider[z] = auto5[z];
        gEngine.params.zoneLevel[z].store(auto5[z]);
    }
    gEngine.params.pacerHz.store(pr.pacerHz);
    gEngine.params.pacerDepth.store(pr.pacerDepth);
    applyPresetTuning(pr.tuning);
}

#ifdef __APPLE__
extern "C" bool macWindowIsFullscreen(GLFWwindow*);
extern "C" void macWindowExitFullscreen(GLFWwindow*);

#include <mach-o/dyld.h>
#include <libgen.h>
#include <sys/stat.h>
// bundled Resources/shaders when present, else the dev checkout
static std::string shaderLibraryDir() {
    char exe[2048];
    uint32_t sz = sizeof(exe);
    if (_NSGetExecutablePath(exe, &sz) == 0) {
        std::string dir = dirname(exe);
        std::string bundled = dir + "/../Resources/shaders";
        struct stat st{};
        if (stat((bundled + "/manifest.json").c_str(), &st) == 0) return bundled;
    }
    return "/Users/lu/ShaderClaw3/shaders";
}
#else
// Windows: GLFW covers fullscreen state; the Cocoa green-button case is moot
static bool macWindowIsFullscreen(GLFWwindow*) { return false; }
static void macWindowExitFullscreen(GLFWwindow*) {}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
// shaders/ folder shipped next to the exe
static std::string shaderLibraryDir() {
    char exe[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir(exe);
    size_t slash = dir.find_last_of("\\/");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    return dir + "\\shaders";
}
#endif

// selftest tone helpers (path + async playback), per platform
static std::string selftestTonePath() {
#ifdef _WIN32
    char buf[MAX_PATH] = {0};
    GetTempPathA(MAX_PATH, buf);
    return std::string(buf) + "ava_selftest_tone.wav";
#else
    return "/tmp/ava_selftest_tone.wav";
#endif
}
static void playToneAsync(const std::string& path) {
#ifdef _WIN32
    PlaySoundA(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC);
#else
    system(("afplay " + path + " > /dev/null 2>&1 &").c_str());
#endif
}

#ifdef __APPLE__
// live input follows the engine's device: on = capture that interface's inputs
static void setInput(bool on) {
    gInOn = on;
    gIn.stop();
    gOut.inRing = nullptr;
    if (!on || gSelDevice < 0 || gSelDevice >= (int)gDevices.size()) return;
    gInRing.init(48000);
    if (gIn.start(gDevices[gSelDevice].id, &gInRing)) {
        gOut.inRing = &gInRing;
        fprintf(stderr, "[input] live input on %s: %d inputs, using %d/%d\n",
                gDevices[gSelDevice].name.c_str(), gIn.deviceChannels, gIn.chanL.load() + 1, gIn.chanR.load() + 1);
    } else fprintf(stderr, "[input] %s\n", gIn.lastError.c_str());
}
#endif
static void startAudio() {
    if (!gTap.running()) gTap.start(&gRing);
    if (!gOut.running() && gSelDevice >= 0 && gSelDevice < (int)gDevices.size())
        gOut.start(gDevices[gSelDevice].id, &gEngine, &gRing);
#ifdef __APPLE__
    if (gInOn) setInput(true);   // follow the device
#endif
    gPlaying = gOut.running();
}
static void stopAudio() {
    gOut.stop();
    gPlaying = false;
}

// ── octagon visualization ──
// Build a closed path with rounded corners: at each vertex, straight edges
// stop short by `r` and a quadratic bezier turns the corner.
static void roundedPolyPath(ImDrawList* dl, const ImVec2* p, int n, float r) {
    // Equal VISUAL radius at every corner: the offset along each edge is
    // corrected by the corner angle (t = r / tan(θ/2)). A fixed offset makes
    // obtuse corners (the inner edge of each ring slice) turn visibly rounder
    // than sharp ones; the correction keeps the whole octagon uniform.
    for (int i = 0; i < n; i++) {
        const ImVec2& cur = p[i];
        const ImVec2& prev = p[(i + n - 1) % n];
        const ImVec2& next = p[(i + 1) % n];
        float lp = std::hypot(prev.x - cur.x, prev.y - cur.y);
        float ln = std::hypot(next.x - cur.x, next.y - cur.y);
        if (lp < 1e-4f) lp = 1e-4f;
        if (ln < 1e-4f) ln = 1e-4f;
        float upx = (prev.x - cur.x) / lp, upy = (prev.y - cur.y) / lp;
        float unx = (next.x - cur.x) / ln, uny = (next.y - cur.y) / ln;
        float cosT = std::max(-0.999f, std::min(0.999f, upx * unx + upy * uny));
        float half = std::acos(cosT) * 0.5f;
        float t = r / std::max(0.20f, std::tan(half));
        float rr = std::min(t, std::min(lp, ln) * 0.35f);
        ImVec2 a(cur.x + upx * rr, cur.y + upy * rr);
        ImVec2 b(cur.x + unx * rr, cur.y + uny * rr);
        dl->PathLineTo(a);
        dl->PathBezierQuadraticCurveTo(cur, b);
    }
}
static void roundedPolyFill(ImDrawList* dl, const ImVec2* p, int n, float r,
                            ImU32 fill, ImU32 stroke, float strokeW) {
    roundedPolyPath(dl, p, n, r);
    dl->PathFillConvex(fill);
    roundedPolyPath(dl, p, n, r);
    dl->PathStroke(stroke, ImDrawFlags_Closed, strokeW);
}

// pad under the mouse, or -1. Uses the octagon's own geometry (corrects for
// the flat edges) so presses near an edge midpoint land in the right ring.
// outK = slice index 0-7, outFrac = radial position within the ring band 0-1.
static int octagonPadAt(ImVec2 c, float R, ImVec2 m, int* outK, float* outFrac) {
    // grounded layout, matching the MIDI pad order: FEET at the center,
    // then ROOT→BELLY→HEART outward, HEAD at the rim
    const int ringZone[4] = {ROOT, BELLY, HEART, HEAD};
    const float bounds[5] = {0.36f, 0.50f, 0.65f, 0.80f, 0.96f};
    *outK = 0;
    *outFrac = 0.5f;
    float dx = m.x - c.x, dy = m.y - c.y;
    float dist = std::hypot(dx, dy);
    if (dist < 1.0f) return FEET;
    float theta = std::atan2(dy, dx);
    // slice k spans [vert k, vert k+1] starting at -pi/2 + pi/8
    float rel = theta - (-dsp::kPi / 2 + dsp::kPi / 8);
    rel -= std::floor(rel / (2 * dsp::kPi)) * 2 * dsp::kPi;
    *outK = std::min(7, (int)(rel / (dsp::kPi / 4)));
    // angular offset from the nearest edge midpoint, in [-pi/8, pi/8]
    float u = (theta - (-dsp::kPi / 2 + dsp::kPi / 4)) / (dsp::kPi / 4);
    float delta = (u - std::round(u)) * (dsp::kPi / 4);
    // fraction of R at which the polygon boundary sits in this direction
    float f = dist * std::cos(delta) / (R * std::cos(dsp::kPi / 8));
    if (f < bounds[0]) return FEET;
    for (int ring = 0; ring < 4; ring++)
        if (f < bounds[ring + 1]) {
            *outFrac = (f - bounds[ring]) / (bounds[ring + 1] - bounds[ring]);
            return ringZone[ring];
        }
    return -1;
}

// level each zone gets back when its ring is right-clicked on again
static float gZoneRestore[NZONES] = {0.65f, 0.6f, 0.8f, 0.75f, 0.75f};
// pad currently held by the mouse (MIDI-style play on the octagon)
static int gPadZone = -1, gPadK = -1;
static bool gPadDrag = false;

// ── MIDI demo mode ──
// BODY CHORD tuning: just-intonation stack on 40 Hz (FEET 1/1 · ROOT 9/8 ·
// BELLY 5/4 · HEART 3/2 · HEAD 2/1) — the all-zone SLAM plays a major triad
// through the body. Live values live in gTuneHz (TUNER panel), indexed by
// Zone (HEAD..FEET); BODY CHORD is the default tuning.
// bottom pad row, GM kick position first: notes 36..40 → FEET..HEAD
static const int kMidiPadZone[5] = {FEET, ROOT, BELLY, HEART, HEAD};
#ifdef __APPLE__
static MidiIn gMidi;
#endif
static int gMidiHeldNote[NZONES] = {-1, -1, -1, -1, -1};
static float gMidiBaseVel[NZONES] = {0};
static float gMidiBaseHz[NZONES] = {0};
static bool gMidiSustain = false;
static bool gMidiSustained[NZONES] = {false};
static float gTestPulse[NZONES] = {0}; // timed pulses (test / slam / wave), s left
static float gWaveT = -1.0f;           // wave scheduler clock, <0 idle
static int gWaveDir = 1;               // 1 = feet→head
static float gWaveVel = 1.0f;
static double gMidiRescanT = 0;
static uint32_t gMidiActPrev = 0;
static double gMidiActFlashT = -10;

// ── TUNER — wavetuner-style body tuner ──
// Five orbs hang from a 20–200 Hz log ruler, one per zone. Drag an orb to
// retune what that zone's pads/slam/wave play (shift = fine), right-click or
// DRONE holds it sounding so you tune by feel, beat readouts show the throb
// rate between neighbours. LEARN binds any CC knob to a zone's Hz or level.
static float gTuneHz[NZONES] = {80.f, 60.f, 50.f, 45.f, 40.f}; // live pad tuning
static bool gTuneDrone[NZONES] = {false};
static int gTuneDrag = -1;           // orb under the mouse button
static bool gTuneAudition = false;   // the drag gated a non-droning zone
static float gTuneDragHz0 = 0, gTuneDragX0 = 0;
static bool gTuneDragShift = false;
static bool gLearnArmed = false;
static int gLearnSel = -1;           // zone awaiting a CC; +NZONES = level target
struct CcMap { int cc, chan, zone, target; }; // target 0 = Hz, 1 = level
static std::vector<CcMap> gCcMaps;
static double gCcFlashT[NZONES][2] = {{-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}, {-10, -10}};
static double gTunerDirtyT = 0;      // >0: unsaved change, save 1 s after last edit
static const float kTuneMin = 20.0f, kTuneMax = 200.0f;
// tuner column order (left→right), FEET first like the octagon's center pad
static const int kTuneOrder[NZONES] = {FEET, ROOT, BELLY, HEART, HEAD};

struct TuningDef { const char* name; const char* note; float hz[NZONES]; }; // HEAD..FEET
static const TuningDef kTunings[] = {
    {"BODY CHORD", "just major on 40 Hz: 1/1 · 9/8 · 5/4 · 3/2 · 2/1", {80, 60, 50, 45, 40}},
    {"WELL-TUNED", "7-limit, after La Monte Young: 1/1 · 9/8 · 21/16 · 3/2 · 7/4", {70, 60, 52.5f, 45, 40}},
    {"BEAT 3", "neighbours 3 Hz apart: the whole body throbs at 3 Hz", {52, 49, 46, 43, 40}},
    {"UNISON", "every zone on 40 Hz: one coherent field, no beating", {40, 40, 40, 40, 40}},
    // CALM: just intonation on a 32 Hz root, every ratio consonant, the top
    // held at a fifth so nothing beats faster than the breath
    {"CALM", "just stack on 32 Hz: 1/1 · 5/4 · 3/2 · 15/8 · 2/1 — consonant, no fast beating", {64, 60, 48, 40, 32}},
};
static const int kNumTunings = sizeof(kTunings) / sizeof(kTunings[0]);
static void setZoneHz(int z, float hz);
static void applyPresetTuning(int t) {
    if (t < 0 || t >= kNumTunings) return;
    for (int z = 0; z < NZONES; z++) setZoneHz(z, kTunings[t].hz[z]);
}

static float ccToHz(int v) { return kTuneMin * std::pow(kTuneMax / kTuneMin, v / 127.0f); }
static void outCC(int ch, int cc, float v01);
static int outChanFor(int z);
static int zoneIdx(int z);
// true when something other than the caller is keeping this zone's pad gated
static bool zoneHeldElsewhere(int z) {
    return gMidiHeldNote[z] >= 0 || gMidiSustained[z] || gPadZone == z || gTuneDrone[z];
}
// retune a zone: pad map, slam, wave and any pad currently sounding follow
static void setZoneHz(int z, float hz) {
    hz = std::max(kTuneMin, std::min(std::min(kTuneMax, zoneHzMax(z)), hz));
    gTuneHz[z] = hz;
    outCC(outChanFor(z), 40 + zoneIdx(z), std::log(hz / kTuneMin) / std::log(kTuneMax / kTuneMin));
    bool padNote = gMidiHeldNote[z] >= 36 && gMidiHeldNote[z] <= 40;
    if (gTuneDrone[z] || gTuneDrag == z || padNote || gTestPulse[z] > 0.0f) {
        if (padNote) gMidiBaseHz[z] = hz;
        gEngine.params.padHz[z].store(hz);
    }
}
static void armZoneVoice(int z, int patch = -1);
static void setDrone(int z, bool on) {
    gTuneDrone[z] = on;
    if (on) {
        armZoneVoice(z, PAD_PURE); // drones are clean tuner tones
        gEngine.params.padHz[z].store(gTuneHz[z]);
        gEngine.params.padVel[z].store(0.9f);
        gEngine.params.padGate[z].store(1);
    } else if (!zoneHeldElsewhere(z) && gTuneDrag != z) {
        gEngine.params.padGate[z].store(0);
    }
}
static std::string tunerStatePath() {
#ifdef _WIN32
    return "ava_tuner.txt";
#else
    const char* home = getenv("HOME");
    std::string dir = std::string(home ? home : ".") + "/Library/Application Support/AVA OS";
    mkdir(dir.c_str(), 0755);
    return dir + "/tuner.txt";
#endif
}
static void saveTunerState() {
    FILE* f = fopen(tunerStatePath().c_str(), "w");
    if (!f) return;
    for (int z = 0; z < NZONES; z++) fprintf(f, "hz %d %.3f\n", z, gTuneHz[z]);
    for (auto& m : gCcMaps) fprintf(f, "cc %d %d %d %d\n", m.cc, m.chan, m.zone, m.target);
    fclose(f);
}
static void loadTunerState() {
    FILE* f = fopen(tunerStatePath().c_str(), "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof(line), f)) {
        int z = -1, a = 0, b = 0, c = 0;
        float hz = 0;
        if (sscanf(line, "hz %d %f", &z, &hz) == 2 && z >= 0 && z < NZONES)
            gTuneHz[z] = std::max(kTuneMin, std::min(kTuneMax, hz));
        else if (sscanf(line, "cc %d %d %d %d", &a, &b, &z, &c) == 4 && z >= 0 && z < NZONES)
            gCcMaps.push_back({a, b, z, c ? 1 : 0});
    }
    fclose(f);
}
// ── OCTAGON → MIDI OUT ──
// The octagon is a playable controller for anything that listens to MIDI:
// each ring is an octave (FEET centre = root, ROOT ring = octave 0 … HEAD
// ring = octave 3), each of the 8 slices a scale step, distance from the
// rim = velocity, angle inside the slice = pitch bend, hold time = pressure.
// Zone sliders, engine params, master and tuner orbs go out as CCs.
#ifdef __APPLE__
static MidiOut gMidiOut;
#endif
static bool gOutOn = true;
static bool gOutZoneCh = false;      // per-zone channels (1 = FEET … 5 = HEAD) vs all on 1
static int gOutRoot = 36;            // C2 at the centre pad
static int gOutMinor = 0;            // 0 major · 1 natural minor
static int gOutNote = -1, gOutCh = 0; // note the octagon is holding
static int gOutBend = 8192, gOutPress = -1;
static double gOutHoldT0 = 0;
static uint32_t gOutActPrev = 0;
static double gOutFlashT = -10;
static char gOutLast[64] = "";
static int gOutCcLast[16][128];      // last value per (ch, cc): dedupe, no flooding
static const int kScaleMajor[8] = {0, 2, 4, 5, 7, 9, 11, 12};
static const int kScaleMinor[8] = {0, 2, 3, 5, 7, 8, 10, 12};
static const struct { const char* label; int cc; } kParamCc[] = {
    {"Intensity", 30}, {"Balance", 31}, {"Pulse", 32}, {"Flow", 33}, {"Warmth", 34},
    {"Depth", 35}, {"Breath", 36}, {"Dynamics", 37}, {"Void", 38}};

static int zoneIdx(int z) { // 0 = FEET … 4 = HEAD (the "1..5" numbering)
    for (int i = 0; i < NZONES; i++) if (kTuneOrder[i] == z) return i;
    return 0;
}
static int outChanFor(int z) { return gOutZoneCh ? zoneIdx(z) : 0; }
static int octNote(int z, int k) {
    if (z == FEET) return gOutRoot;
    const int* sc = gOutMinor ? kScaleMinor : kScaleMajor;
    return std::min(127, gOutRoot + 12 * (zoneIdx(z) - 1) + sc[k & 7]);
}
static void outLog(const char* fmt, int a, int b, int c) {
    snprintf(gOutLast, sizeof(gOutLast), fmt, a, b, c);
}
static void outNoteOff() {
    if (gOutNote < 0) return;
#ifdef __APPLE__
    if (gOutOn && gMidiOut.running()) {
        if (gOutBend != 8192) gMidiOut.pitchBend(gOutCh, 8192);
        if (gOutPress > 0) gMidiOut.pressure(gOutCh, 0);
        gMidiOut.noteOff(gOutCh, gOutNote);
        snprintf(gOutLast, sizeof(gOutLast), "note off %d  ch %d", gOutNote, gOutCh + 1);
    }
#endif
    gOutNote = -1;
    gOutBend = 8192;
    gOutPress = -1;
}
static void outNoteOn(int ch, int note, float vel01) {
    gOutNote = note;
    gOutCh = ch;
    gOutBend = 8192;
    gOutPress = -1;
    gOutHoldT0 = glfwGetTime();
#ifdef __APPLE__
    if (gOutOn && gMidiOut.running()) {
        int v = (int)std::lround(1 + 126 * std::max(0.0f, std::min(1.0f, vel01)));
        gMidiOut.noteOn(ch, note, v);
        outLog("note %d  vel %d  ch %d", note, v, ch + 1);
    }
#endif
}
// expression while a pad is held: bend from the angle inside the slice
// (±1 semitone, assuming the receiver's default ±2 range), pressure swells
// with hold time like the engine's own press-and-hold surge
static void outExpress(float sliceFrac, double now) {
    if (gOutNote < 0) return;
    int bend = 8192 + (int)std::lround((sliceFrac - 0.5f) * 2.0f * 4096.0f);
    int press = (int)std::lround(std::min(1.0, (now - gOutHoldT0) / 3.0) * 127.0);
#ifdef __APPLE__
    if (gOutOn && gMidiOut.running()) {
        if (std::abs(bend - gOutBend) >= 24) { gMidiOut.pitchBend(gOutCh, bend); gOutBend = bend; }
        if (press != gOutPress) { gMidiOut.pressure(gOutCh, press); gOutPress = press; }
    }
#else
    (void)bend; (void)press;
#endif
}
static void outCC(int ch, int cc, float v01) {
    int v = (int)std::lround(127.0f * std::max(0.0f, std::min(1.0f, v01)));
    if (gOutCcLast[ch & 15][cc & 127] == v) return;
    gOutCcLast[ch & 15][cc & 127] = v;
#ifdef __APPLE__
    if (gOutOn && gMidiOut.running()) {
        gMidiOut.cc(ch, cc, v);
        outLog("CC %d = %d  ch %d", cc, v, ch + 1);
    }
#endif
}
static void outParamCC(const char* label, float v01) {
    for (auto& pc : kParamCc)
        if (std::strcmp(pc.label, label) == 0) { outCC(0, pc.cc, v01); return; }
}

static void hzToNote(float hz, char* out, size_t n) {
    float m = 69.0f + 12.0f * std::log2(hz / 440.0f);
    int nn = (int)std::lround(m);
    int cents = (int)std::lround((m - nn) * 100.0f);
    snprintf(out, n, "%s%d %+dc", kKeyNames[((nn % 12) + 12) % 12], nn / 12 - 1, cents);
}

// ── system health monitor ──
static bool gShowHealth = false;
static int gSweepZone = -1;               // TEST ALL sweep: current zone, <0 idle
static double gChanLastLive[OutputUnit::kMaxCh] = {0}; // last time signal seen
static double gHealthScanT = 0;           // periodic device-presence rescan
static bool gDevPresent = true;           // selected interface still connected

// every strike carries the selected voice into the zone it hits
static void armZoneVoice(int z, int patch) {
    if (patch < 0) patch = gEngine.params.padPatch.load();
    gEngine.params.padPatchZ[z].store(std::min(std::max(patch, 0), NPATCHES - 1));
}
static void fireZonePulse(int z) {
    armZoneVoice(z);
    float hz = std::max(zoneSweetLo(z), std::min(zoneSweetHi(z), gEngine.zoneHz[z].load()));
    gEngine.params.padHz[z].store(hz);
    gEngine.params.padVel[z].store(1.0f);
    gEngine.params.padGate[z].store(1);
    gTestPulse[z] = 0.9f;
}

// ring isolation: solo one zone (toggle off when soloing the same zone again)
// and kick it with a pulse so the isolated transducer is felt immediately
static void setSoloZone(int z) {
    int cur = gEngine.params.soloZone.load();
    int next = (z < 0 || z == cur) ? -1 : z;
    gEngine.params.soloZone.store(next);
    if (next >= 0) fireZonePulse(next);
}

// everything we control pushed to its ceiling: engine master, music/surround
// sends, all zone trims, and the interface's own hardware volume scalars
static void maxAllLevels() {
    gMasterVol = 1.0f;
    gEngine.params.masterVolume.store(1.0f);
    gEngine.params.musicGain.store(1.5f);
    gEngine.params.surrGain.store(1.5f);
    for (int z = 0; z < NZONES; z++) gEngine.params.zoneTrim[z].store(1.5f);
    if (gSelDevice >= 0 && gSelDevice < (int)gDevices.size())
        maxDeviceHwVolume(gDevices[gSelDevice].id);
}

// demo velocity curve: floor at 30% so a shy tap still clearly thumps
static float velCurve(int v) {
    float x = v / 127.0f;
    return 0.30f + 0.70f * std::pow(x, 1.5f);
}

static void drawOctagon(ImDrawList* dl, ImVec2 c, float R) {
    // zone per ring: center pad = FEET, then ROOT→HEAD outward (grounded
    // layout — same order as the MIDI zone pads)
    const int ringZone[4] = {ROOT, BELLY, HEART, HEAD};
    const float bounds[5] = {0.36f, 0.50f, 0.65f, 0.80f, 0.96f};
    // white glass slices — opacity steps per ring (outer most transparent) so
    // the shader background glows through the body
    const int ringAlpha[4] = {215, 185, 155, 125};
    float roundR = std::max(5.0f, R * 0.030f);

    // octagon as MIDI controller: left-press strikes the pad under the mouse
    // (slice = pitch step, radial position = velocity), drag glides across
    // pads, release rings out. Right-click toggles the zone's vibration.
    int hoverZone = -1, hoverK = 0;
    float hoverFrac = 0.5f;
    // (while a pad drag is in progress ImGui holds the root window's MoveId as
    // the active id, so only block on a *real* item being active before the press)
    if (!ImGui::IsAnyItemHovered() && (gPadDrag || !ImGui::IsAnyItemActive()))
        hoverZone = octagonPadAt(c, R, ImGui::GetIO().MousePos, &hoverK, &hoverFrac);
    if (hoverZone >= 0) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsMouseClicked(1)) {
            if (gZoneSlider[hoverZone] > 0.001f) {
                gZoneRestore[hoverZone] = gZoneSlider[hoverZone];
                gZoneSlider[hoverZone] = 0.0f;
            } else {
                gZoneSlider[hoverZone] =
                    gZoneRestore[hoverZone] > 0.001f ? gZoneRestore[hoverZone] : 0.6f;
            }
            gEngine.params.zoneLevel[hoverZone].store(gZoneSlider[hoverZone]);
        }
        if (ImGui::IsMouseClicked(0)) gPadDrag = true;
    }
    if (gPadDrag && ImGui::IsMouseDown(0) && hoverZone >= 0) {
        // pitch: patches with a fixed body frequency step semitones per slice;
        // zone-following patches step one octave across the 8 slices, capped
        // at the 200 Hz zone lowpass. Velocity: nearer the outer edge = harder
        int patch = std::min(std::max(gEngine.params.padPatch.load(), 0), NPATCHES - 1);
        float pbase = kPadPatches[patch].baseHz;
        float hz = pbase > 0
            ? pbase * std::pow(2.0f, hoverK / 12.0f)
            : std::min(zoneSweetHi(hoverZone), std::min(45.0f, gEngine.zoneHz[hoverZone].load()) *
                                   std::pow(2.0f, hoverK / 8.0f));
        float vel = 0.55f + 0.45f * hoverFrac;
        // WHALE is a full-body surge (in the source stems every zone peaks
        // together); the other patches play the pressed zone only
        bool allZones = kPadPatches[patch].allZones;
        if (gPadZone >= 0 && gPadZone != hoverZone && !allZones && !gTuneDrone[gPadZone])
            gEngine.params.padGate[gPadZone].store(0);
        for (int z = 0; z < NZONES; z++) {
            if (!allZones && z != hoverZone) continue;
            armZoneVoice(z);
            gEngine.params.padHz[z].store(hz);
            gEngine.params.padVel[z].store(vel);
            gEngine.params.padGate[z].store(1);
        }
        gPadZone = hoverZone;
        gPadK = hoverK;

        // MIDI out: new pad = new note; angle inside the slice bends, hold swells
        {
            int note = octNote(hoverZone, hoverK);
            int ch = outChanFor(hoverZone);
            if (gOutNote != note || gOutCh != ch) {
                outNoteOff();
                outNoteOn(ch, note, hoverFrac);
            }
            float sliceFrac = 0.5f;
            if (hoverZone != FEET) {
                ImVec2 m = ImGui::GetIO().MousePos;
                float theta = std::atan2(m.y - c.y, m.x - c.x);
                float rel = theta - (-dsp::kPi / 2 + dsp::kPi / 8);
                rel -= std::floor(rel / (2 * dsp::kPi)) * 2 * dsp::kPi;
                sliceFrac = rel / (dsp::kPi / 4);
                sliceFrac -= std::floor(sliceFrac);
            }
            outExpress(sliceFrac, glfwGetTime());
        }
    } else if (gPadZone >= 0 || !ImGui::IsMouseDown(0)) {
        outNoteOff();
        if (gPadZone >= 0)
            for (int z = 0; z < NZONES; z++) {
                if (gTuneDrone[z]) { // hand the zone back to its drone tuning
                    gEngine.params.padHz[z].store(gTuneHz[z]);
                    gEngine.params.padVel[z].store(0.9f);
                } else if (gMidiHeldNote[z] < 0 && !gMidiSustained[z])
                    gEngine.params.padGate[z].store(0);
            }
        gPadZone = -1;
        gPadK = -1;
        if (!ImGui::IsMouseDown(0)) gPadDrag = false;
    }

    auto vert = [&](float r, int k, float dy = 0.0f) {
        float a = -dsp::kPi / 2 + dsp::kPi / 8 + k * dsp::kPi / 4;
        return ImVec2(c.x + r * R * std::cos(a), c.y + r * R * std::sin(a) + dy);
    };

    // rings, outer first so inner edges overlay cleanly
    for (int ring = 3; ring >= 0; ring--) {
        float lvl = gEngine.meter[2 + ringZone[ring]].load();
        for (int k = 0; k < 8; k++) {
            ImVec2 raw[4] = {vert(bounds[ring + 1], k), vert(bounds[ring + 1], k + 1),
                             vert(bounds[ring], k + 1), vert(bounds[ring], k)};
            // equal fixed padding on every edge (slice↔slice and ring↔ring alike)
            ImVec2 q[4];
            insetConvexPoly(raw, 4, std::max(3.0f, R * 0.011f), q);
            ImVec2 ctr((q[0].x + q[1].x + q[2].x + q[3].x) / 4,
                       (q[0].y + q[1].y + q[2].y + q[3].y) / 4);
            // audio-reactive glass: opacity breathes with the zone level and a
            // slow wave travels around the ring, deeper when the zone is loud
            float tNow = (float)ImGui::GetTime();
            int a = (int)(ringAlpha[ring] * (0.60f + 0.40f * lvl)
                          + 14.0f * std::sin(k * 1.7f + ring)
                          + (8.0f + 30.0f * lvl) *
                                std::sin(tNow * 1.6f + k * 0.785f + ring * 1.3f));
            a = std::min(250, std::max(55, a));
            if (gZoneSlider[ringZone[ring]] < 0.001f) a = (int)(a * 0.45f); // muted
            { int so = gEngine.params.soloZone.load();
              if (so >= 0 && so != ringZone[ring]) a = (int)(a * 0.30f); } // not soloed
            if (hoverZone == ringZone[ring]) a = std::min(252, a + (hoverK == k ? 44 : 12));
            bool held = gPadZone == ringZone[ring] && gPadK == k;
            if (held) a = 252;
            roundedPolyFill(dl, q, 4, roundR, IM_COL32(255, 255, 255, a),
                            held ? IM_COL32(255, 255, 255, 230) : IM_COL32(0, 0, 0, 30),
                            held ? 2.5f : 1.0f);

            // dot embedded in the slice: dark when idle, lit when the zone fires
            float dg = 0.40f + 0.60f * lvl;
            dl->AddCircleFilled(ImVec2(ctr.x, ctr.y + 1.0f), 6.0f, IM_COL32(0, 0, 0, 35));
            dl->AddCircleFilled(ctr, 5.5f,
                IM_COL32((int)(dg * 255), (int)(dg * 255), (int)(dg * 255), 255));
            if (lvl > 0.05f)
                dl->AddCircleFilled(ctr, 9.0f + 6.0f * lvl, W(0.18f * lvl));
        }
    }

    // center pad (FEET)
    {
        float lvl = gEngine.meter[2 + FEET].load();
        ImVec2 raw[8], pts[8];
        for (int k = 0; k < 8; k++) raw[k] = vert(bounds[0], k);
        insetConvexPoly(raw, 8, std::max(3.0f, R * 0.011f), pts);
        int a = 205 + (int)(lvl * 45.0f);
        if (gZoneSlider[FEET] < 0.001f) a = (int)(a * 0.45f); // muted
        { int so = gEngine.params.soloZone.load();
          if (so >= 0 && so != FEET) a = (int)(a * 0.30f); } // not soloed
        if (hoverZone == FEET) a = std::min(252, a + 32);
        bool held = gPadZone == FEET;
        if (held) a = 252;
        roundedPolyFill(dl, pts, 8, roundR * 1.4f,
                        IM_COL32(255, 255, 255, std::min(a, 252)),
                        held ? IM_COL32(255, 255, 255, 230) : IM_COL32(0, 0, 0, 26),
                        held ? 2.5f : 1.0f);
        dl->AddCircleFilled(ImVec2(c.x, c.y + 1.5f), 14.5f, IM_COL32(0, 0, 0, 38));
        float dg = 0.45f + 0.55f * lvl;
        dl->AddCircleFilled(c, 13.0f,
            IM_COL32((int)(dg * 255), (int)(dg * 255), (int)(dg * 255), 255));
        if (lvl > 0.05f) dl->AddCircleFilled(c, 22.0f + 10.0f * lvl, W(0.20f * lvl));
    }
}

// ── custom vertical slider pair (thin live meter + fat slider) ──
static void zoneColumn(int z, float x, float y, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();

    // header
    ImGui::PushFont(nullptr);
    ImVec2 nameSz = ImGui::CalcTextSize(kZoneNames[z]);
    dl->AddText(ImVec2(x + (w - nameSz.x) / 2, y), W(0.92f), kZoneNames[z]);
    char sub[24];
    snprintf(sub, sizeof(sub), "%.0f Hz", gEngine.zoneHz[z].load());
    ImVec2 subSz = ImGui::CalcTextSize(sub);
    dl->AddText(ImVec2(x + (w - subSz.x) / 2, y + 19), W(0.35f), sub);
    ImGui::PopFont();

    float top = y + 46;
    float bot = y + h - 40;          // leaves one row for the S / V readouts
    float sh = bot - top;
    float meterX = x + w / 2 - 14, sliderX = x + w / 2 + 6;

    // thin live meter
    float lvl = gEngine.meter[2 + z].load();
    dl->AddRectFilled(ImVec2(meterX - 1.5f, top), ImVec2(meterX + 1.5f, bot), W(0.10f), 2);
    dl->AddRectFilled(ImVec2(meterX - 1.5f, bot - sh * lvl), ImVec2(meterX + 1.5f, bot), W(0.85f), 2);

    // fat slider
    float v = gZoneSlider[z];
    dl->AddRectFilled(ImVec2(sliderX - 5, top), ImVec2(sliderX + 5, bot), W(0.10f), 5);
    float knobY = bot - sh * v;
    // gradient-ish fill: brighter near knob
    dl->AddRectFilledMultiColor(ImVec2(sliderX - 5, knobY), ImVec2(sliderX + 5, bot),
                                W(0.95f), W(0.95f), W(0.55f), W(0.55f));
    dl->AddCircleFilled(ImVec2(sliderX, knobY), 10.0f, W(0.97f));

    // drag handling
    ImGui::SetCursorScreenPos(ImVec2(sliderX - 14, top - 10));
    char id[16]; snprintf(id, sizeof(id), "##zs%d", z);
    ImGui::InvisibleButton(id, ImVec2(28, sh + 20));
    if (ImGui::IsItemActive()) {
        float ny = (bot - io.MousePos.y) / sh;
        gZoneSlider[z] = std::max(0.0f, std::min(1.0f, ny));
        gEngine.params.zoneLevel[z].store(gZoneSlider[z]);
        outCC(outChanFor(z), 20 + zoneIdx(z), gZoneSlider[z]);
    }

    // readouts: joined "S 11" / "V 70" under their bars, small type
    if (gFontSmall) ImGui::PushFont(gFontSmall);
    char v1[12], v2[12];
    snprintf(v1, sizeof(v1), "%d", (int)std::lround(v * 100));
    snprintf(v2, sizeof(v2), "%d", (int)std::lround(lvl * 100));
    auto stacked = [&](float cx, float yy, const char* letter, const char* num) {
        ImVec2 ns = ImGui::CalcTextSize(num);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(cx - ns.x / 2, yy), W(0.85f), num);
        ImVec2 ls = ImGui::CalcTextSize(letter);
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                    ImVec2(cx - ls.x / 2, yy + 16), W(0.32f), letter);
    };
    stacked(meterX - 6, bot + 8, "S", v2);
    stacked(sliderX + 10, bot + 8, "V", v1);
    if (gFontSmall) ImGui::PopFont();
}

static void miniParam(const char* label, std::atomic<float>& p, float x, float y, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    dl->AddText(ImVec2(x, y), W(0.35f), label);
    float barY = y + 18, barH = 3;
    float v = p.load();
    dl->AddRectFilled(ImVec2(x, barY), ImVec2(x + w, barY + barH), W(0.10f), 2);
    dl->AddRectFilled(ImVec2(x, barY), ImVec2(x + w * v, barY + barH), W(0.75f), 2);
    dl->AddCircleFilled(ImVec2(x + w * v, barY + barH / 2), 5.5f, W(0.95f));
    ImGui::SetCursorScreenPos(ImVec2(x - 4, barY - 9));
    char id[32]; snprintf(id, sizeof(id), "##mp%s", label);
    ImGui::InvisibleButton(id, ImVec2(w + 8, 20));
    if (ImGui::IsItemActive()) {
        float nv = (io.MousePos.x - x) / w;
        p.store(std::max(0.0f, std::min(1.0f, nv)));
        outParamCC(label, p.load());
    }
}

// strike one zone from MIDI (velocity already curved). Strikes stay in the
// felt band: transducers give almost no tactile output above ~110 Hz, so a
// tuner orb or CC knob parked high must not silence a pad hit.
static void midiStrikeZone(int z, float vel, float hz, int patch = -1) {
    if (patch < 0) patch = gEngine.params.padPatch.load();
    armZoneVoice(z, patch);
    if (kPadPatches[patch].baseHz > 0.0f) hz = kPadPatches[patch].baseHz;
    // every strike lands inside the zone's sweet band (rings 30-70, centre 20-80)
    hz = std::max(zoneSweetLo(z), std::min(zoneSweetHi(z), hz));
    fprintf(stderr, "[MIDI] strike %s %.0f Hz vel %.2f -> ch %d\n", kZoneNames[z], hz, vel,
            gEngine.params.zoneChan[z].load() + 1);
    gEngine.params.padHz[z].store(hz);
    gEngine.params.padVel[z].store(vel);
    gEngine.params.padGate[z].store(1);
}

// ── VFX sequencer ──
// A VFX is a scored list of strikes (time, zone, voice, velocity, hold) that
// rolls across the rings: waves head-to-toe, ripples out from the belly,
// full-body slams. TUNER VFX are the same ideas exaggerated: hotter
// velocities (the engine clips at full scale), stacked voices, sub under it.
struct SeqStep { float t; int zone; int patch; float vel; float hold; };
struct VfxDef {
    const char* name;
    const char* hint;
    bool tuner;
    std::vector<SeqStep> steps;
    float length; // seconds, for the progress bar on the tile
    int icon;     // PadIcon
};
static std::vector<VfxDef> gVfx;
struct SeqEvent { double at; int zone; int patch; float vel; float hold; };
static std::vector<SeqEvent> gSeq;
static int gVfxLast = -1;          // last fired VFX (highlight)
static double gVfxLastT = -10;     // when it fired

static void seqAll(std::vector<SeqStep>& v, float t, int patch, float vel, float hold) {
    for (int z = 0; z < NZONES; z++) v.push_back({t, z, patch, vel, hold});
}
// body order FEET→HEAD (dir 1) or HEAD→FEET (dir -1), one zone per gap
static void seqWave(std::vector<SeqStep>& v, float t, int dir, int patch, float vel,
                    float hold, float gap) {
    for (int i = 0; i < NZONES; i++) {
        int z = dir > 0 ? kTuneOrder[i] : kTuneOrder[NZONES - 1 - i];
        v.push_back({t + i * gap, z, patch, vel, hold});
    }
}
// out from the belly: BELLY, then HEART+ROOT, then HEAD+FEET
static void seqRipple(std::vector<SeqStep>& v, float t, int patch, float vel, float hold,
                      float gap, bool back) {
    v.push_back({t, BELLY, patch, vel, hold});
    v.push_back({t + gap, HEART, patch, vel, hold});
    v.push_back({t + gap, ROOT, patch, vel, hold});
    v.push_back({t + 2 * gap, HEAD, patch, vel, hold});
    v.push_back({t + 2 * gap, FEET, patch, vel, hold});
    if (back) {
        v.push_back({t + 3 * gap, HEART, patch, vel * 0.85f, hold});
        v.push_back({t + 3 * gap, ROOT, patch, vel * 0.85f, hold});
        v.push_back({t + 4 * gap, BELLY, patch, vel * 0.7f, hold});
    }
}
static void buildVfx() {
    if (!gVfx.empty()) return;
    auto add = [&](const char* name, const char* hint, bool tuner, std::vector<SeqStep> st, int icon) {
        float len = 0;
        for (auto& e : st) len = std::max(len, e.t + e.hold);
        gVfx.push_back({name, hint, tuner, std::move(st), len, icon});
    };
    std::vector<SeqStep> v;
    // ── VFX: every ring working together ──
    v.clear(); seqWave(v, 0, 1, PAD_KICK, 0.95f, 0.22f, 0.09f);
    add("WAVE UP", "kicks feet to head", false, v, IC_UP);
    v.clear(); seqWave(v, 0, -1, PAD_KICK, 0.95f, 0.22f, 0.09f);
    add("WAVE DOWN", "kicks head to feet", false, v, IC_DOWN);
    v.clear(); seqRipple(v, 0, PAD_TOM, 0.9f, 0.2f, 0.11f, true);
    add("RIPPLE", "toms out from the belly", false, v, IC_RIPPLE);
    v.clear();
    for (int i = 0; i < 9; i++) { int k = i < 5 ? i : 8 - i; v.push_back({i * 0.08f, kTuneOrder[k], PAD_TOM, 0.9f, 0.18f}); }
    add("BOUNCE", "feet, head, feet, fast", false, v, IC_BOUNCE);
    v.clear();
    v.push_back({0, HEART, PAD_HEART, 1.0f, 1.9f}); v.push_back({0, BELLY, PAD_HEART, 1.0f, 1.9f});
    v.push_back({0.05f, ROOT, PAD_HEART, 0.6f, 1.9f}); v.push_back({0.05f, FEET, PAD_HEART, 0.6f, 1.9f});
    add("HEARTBEAT", "two lub-dubs in the core", false, v, IC_ECG);
    v.clear(); seqWave(v, 0, -1, PAD_TOM, 0.95f, 0.2f, 0.1f);
    v.push_back({0.6f, FEET, PAD_BOOM, 1.0f, 0.8f}); seqWave(v, 0.9f, 1, PAD_KICK, 0.9f, 0.18f, 0.07f);
    add("DRUM SWEEP", "toms down, boom, kicks up", false, v, IC_SWEEP);
    v.clear();
    v.push_back({0, FEET, PAD_BOOM, 1.0f, 0.9f}); v.push_back({0.12f, ROOT, PAD_KICK, 0.95f, 0.22f});
    v.push_back({0.24f, BELLY, PAD_KICK, 0.95f, 0.22f}); v.push_back({0.36f, HEART, PAD_TOM, 0.9f, 0.3f});
    v.push_back({0.48f, HEAD, PAD_SNARE, 0.9f, 0.14f});
    add("CASCADE", "boom to snare up the body", false, v, IC_STAIRS);
    v.clear(); seqAll(v, 0, PAD_QUAKE, 1.0f, 2.5f);
    add("EARTHQUAKE", "2.5 s full-body shake", false, v, IC_ZIGZAG);
    v.clear(); seqAll(v, 0, PAD_WHALE, 1.0f, 4.0f);
    add("WHALE SURGE", "4 s full-body surge", false, v, IC_WHALE);
    v.clear(); seqWave(v, 0, 1, PAD_THUNDER, 0.95f, 2.2f, 0.15f);
    add("THUNDER ROLL", "rumble climbs the body", false, v, IC_BOLT);
    v.clear(); seqAll(v, 0, PAD_BRAAM, 1.0f, 1.5f); v.push_back({0, FEET, PAD_BOOM, 1.0f, 1.0f});
    add("BRAAM HIT", "horn on every ring", false, v, IC_HORN);
    v.clear(); seqAll(v, 0, PAD_RISER, 0.9f, 4.0f); seqAll(v, 4.05f, PAD_IMPACT, 1.0f, 1.8f);
    add("RISE & DROP", "4 s climb, then the hit", false, v, IC_RISEDROP);
    // ── TUNER VFX: exaggerated ──
    v.clear(); seqWave(v, 0, 1, PAD_KICK, 1.4f, 0.3f, 0.09f); seqWave(v, 0.06f, 1, PAD_BOOM, 0.9f, 0.5f, 0.09f);
    add("MEGA WAVE UP", "kick + boom, feet to head", true, v, IC_UP);
    v.clear(); seqWave(v, 0, -1, PAD_KICK, 1.4f, 0.3f, 0.09f); seqWave(v, 0.06f, -1, PAD_BOOM, 0.9f, 0.5f, 0.09f);
    add("MEGA WAVE DOWN", "kick + boom, head to feet", true, v, IC_DOWN);
    v.clear(); seqRipple(v, 0, PAD_IMPACT, 1.3f, 0.5f, 0.11f, true);
    add("MEGA RIPPLE", "impacts out from the belly", true, v, IC_RIPPLE);
    v.clear(); seqAll(v, 0, PAD_IMPACT, 1.4f, 2.0f); v.push_back({0, FEET, PAD_BOOM, 1.4f, 1.2f}); seqAll(v, 0.2f, PAD_BRAAM, 1.0f, 2.5f);
    add("FULL BODY SLAM", "impact, boom, horn at once", true, v, IC_SLAM);
    v.clear(); seqAll(v, 0, PAD_QUAKE, 1.4f, 4.0f);
    for (int i = 0; i < 8; i++) v.push_back({i * 0.5f, FEET, PAD_BOOM, 1.2f, 0.4f});
    add("DEEP QUAKE", "4 s quake over booms", true, v, IC_ZIGZAG);
    v.clear(); seqAll(v, 0, PAD_RISER, 1.2f, 4.0f); seqAll(v, 4.05f, PAD_IMPACT, 1.4f, 2.0f);
    seqWave(v, 4.3f, -1, PAD_TOM, 1.2f, 0.25f, 0.1f); v.push_back({4.9f, FEET, PAD_BOOM, 1.4f, 1.5f});
    add("TSUNAMI", "climb, hit, roll down, boom", true, v, IC_RISEDROP);
    v.clear(); seqAll(v, 0, PAD_HEART, 1.4f, 3.6f);
    add("HEART MAX", "hot lub-dub on every ring", true, v, IC_HEART);
    v.clear();
    { static const int hop[8] = {FEET, HEAD, ROOT, HEART, BELLY, FEET, HEAD, BELLY};
      for (int i = 0; i < 16; i++) v.push_back({i * 0.07f, hop[i % 8], (i & 1) ? PAD_TOM : PAD_KICK, 1.3f, 0.15f}); }
    seqAll(v, 1.2f, PAD_BOOM, 1.4f, 1.0f);
    add("DRUM STORM", "16 hits, then boom", true, v, IC_STORM);
}
static void fireVfx(int idx) {
    buildVfx();
    if (idx < 0 || idx >= (int)gVfx.size()) return;
    double now = glfwGetTime();
    for (auto& st : gVfx[idx].steps) gSeq.push_back({now + st.t, st.zone, st.patch, st.vel, st.hold});
    gVfxLast = idx;
    gVfxLastT = now;
    fprintf(stderr, "[VFX] %s\n", gVfx[idx].name);
}
static void stopVfx() {
    gSeq.clear();
    for (int z = 0; z < NZONES; z++) gTestPulse[z] = std::min(gTestPulse[z], 0.05f);
}
// fire every event that has come due (called once per frame)
static void advanceSequencer(double now) {
    for (size_t i = 0; i < gSeq.size();) {
        if (gSeq[i].at <= now) {
            SeqEvent e = gSeq[i];
            float hz = kPadPatches[e.patch].baseHz > 0 ? kPadPatches[e.patch].baseHz : gTuneHz[e.zone];
            midiStrikeZone(e.zone, e.vel, hz, e.patch);
            gTestPulse[e.zone] = std::max(gTestPulse[e.zone], e.hold);
            gSeq.erase(gSeq.begin() + i);
        } else i++;
    }
}
// audition a single voice: strike the core (HEART + BELLY), full-body voices everywhere
static void auditionVoice(int patch) {
    const PadPatchDef& pd = kPadPatches[patch];
    float hold = pd.cat == CAT_DRUM ? 0.25f : (pd.atkS > 1.0f ? 3.5f : 1.2f);
    for (int z = 0; z < NZONES; z++) {
        if (!pd.allZones && z != HEART && z != BELLY) continue;
        midiStrikeZone(z, 0.95f, pd.baseHz > 0 ? pd.baseHz : gTuneHz[z], patch);
        gTestPulse[z] = std::max(gTestPulse[z], hold);
    }
}

// drain MIDI, run the pad map, advance wave/pulse schedulers. Called once
// per frame; all engine access is via atomics so this is thread-clean.
static void processMidi(double now, float dt) {
#ifdef __APPLE__
    // hot-plug: cheap rescan every 2 s (same cadence as the camera picker)
    if (now - gMidiRescanT > 2.0) {
        gMidiRescanT = now;
        gMidi.rescan();
    }

    for (const MidiEvent& e : gMidi.poll()) {
        if (e.type == 0x90) {
            int n = e.a;
            float vel = velCurve(e.b);
            if (n >= 36 && n <= 40) { // zone pads, bottom row
                int z = kMidiPadZone[n - 36];
                gMidiHeldNote[z] = n;
                gMidiSustained[z] = false;
                gMidiBaseVel[z] = vel;
                gMidiBaseHz[z] = gTuneHz[z];
                gTestPulse[z] = 0;
                midiStrikeZone(z, vel, gTuneHz[z]);
            } else if (n == 41) { // ALL SLAM: body triad, chokes everything sounding
                for (int z = 0; z < NZONES; z++) {
                    gMidiHeldNote[z] = -1;
                    gMidiSustained[z] = false;
                    midiStrikeZone(z, vel, gTuneHz[z]);
                    gTestPulse[z] = 0.55f;
                }
            } else if (n == 42 || n == 43) { // WAVE up / down the body
                gWaveT = 0;
                gWaveDir = (n == 42) ? 1 : -1;
                gWaveVel = vel;
            } else if (n == 44) { // STROBE — momentary while held
                gEngine.params.fxStrobe.store(1);
            } else if (n == 47) { // CHOKE — momentary while held
                gEngine.params.fxChoke.store(1);
            } else if (n >= 48 && n <= 67) { // second pad rows: 48-59 VFX, 60-67 TUNER VFX
                buildVfx();
                int vi = 0, ti = 0, target = -1;
                for (int i = 0; i < (int)gVfx.size(); i++) {
                    if (!gVfx[i].tuner) { if (n - 48 == vi) target = i; vi++; }
                    else { if (n >= 60 && n - 60 == ti) target = i; ti++; }
                }
                if (target >= 0) fireVfx(target);
            } else if (n != 45 && n != 46) {
                // chromatic fallback: any keyboard note → tuned band → zone,
                // so a piano is a body-glissando instrument (and a per-zone
                // frequency tuner) for free
                float hz = 440.0f * std::pow(2.0f, (n - 69) / 12.0f);
                hz = std::max(20.0f, std::min(120.0f, hz));
                int z = hz < 42.0f ? FEET : hz < 48.0f ? ROOT : hz < 56.0f ? BELLY
                        : hz < 70.0f ? HEART : HEAD;
                gMidiHeldNote[z] = n;
                gMidiSustained[z] = false;
                gMidiBaseVel[z] = vel;
                gMidiBaseHz[z] = hz;
                gTestPulse[z] = 0;
                midiStrikeZone(z, vel, hz);
            }
        } else if (e.type == 0x80) {
            int n = e.a;
            if (n == 44) gEngine.params.fxStrobe.store(0);
            else if (n == 47) gEngine.params.fxChoke.store(0);
            else
                for (int z = 0; z < NZONES; z++)
                    if (gMidiHeldNote[z] == n) {
                        gMidiHeldNote[z] = -1;
                        if (gMidiSustain) gMidiSustained[z] = true;
                        else if (!zoneHeldElsewhere(z)) gEngine.params.padGate[z].store(0);
                    }
        } else if (e.type == 0xB0) {
            if (gLearnSel >= 0) { // LEARN: the first knob moved binds to the armed cell
                int z = gLearnSel % NZONES, tgt = gLearnSel / NZONES;
                for (auto it = gCcMaps.begin(); it != gCcMaps.end();)
                    if ((it->cc == e.a && it->chan == e.channel) ||
                        (it->zone == z && it->target == tgt))
                        it = gCcMaps.erase(it);
                    else
                        ++it;
                gCcMaps.push_back({e.a, e.channel, z, tgt});
                gLearnSel = -1;
                gLearnArmed = false;
                saveTunerState();
                continue;
            }
            bool mapped = false;
            for (auto& m : gCcMaps)
                if (m.cc == e.a && m.chan == e.channel) {
                    mapped = true;
                    gCcFlashT[m.zone][m.target] = now;
                    if (m.target == 0) {
                        setZoneHz(m.zone, ccToHz(e.b));
                        gTunerDirtyT = now;
                    } else {
                        gZoneSlider[m.zone] = e.b / 127.0f;
                        gEngine.params.zoneLevel[m.zone].store(gZoneSlider[m.zone]);
                    }
                }
            if (mapped) continue; // a learned knob overrides the stock CC map
            if (e.a == 1) { // mod wheel → tremolo depth, rate fixed 5.5 Hz
                gEngine.params.fxTrem.store(e.b / 127.0f * 0.6f);
            } else if (e.a == 7) { // master trim
                gMasterVol = e.b / 127.0f;
                gEngine.params.masterVolume.store(gMasterVol);
            } else if (e.a == 64) { // sustain pedal
                gMidiSustain = e.b >= 64;
                if (!gMidiSustain)
                    for (int z = 0; z < NZONES; z++)
                        if (gMidiSustained[z]) {
                            gMidiSustained[z] = false;
                            if (!zoneHeldElsewhere(z))
                                gEngine.params.padGate[z].store(0);
                        }
            }
        } else if (e.type == 0xD0) { // aftertouch: lean in → held zones swell
            float at = e.a / 127.0f;
            for (int z = 0; z < NZONES; z++)
                if (gMidiHeldNote[z] >= 0) {
                    float v = gMidiBaseVel[z] * (1.0f + 0.6f * at);
                    gEngine.params.padVel[z].store(v);
                }
        } else if (e.type == 0xE0) { // pitch bend ±5 Hz — beating sweeps
            float bend = (((int)e.b << 7 | e.a) - 8192) / 8192.0f * 5.0f;
            for (int z = 0; z < NZONES; z++)
                if (gMidiHeldNote[z] >= 0 || gMidiSustained[z])
                    gEngine.params.padHz[z].store(
                        std::max(20.0f, gMidiBaseHz[z] + bend));
        }
    }
#endif

    advanceSequencer(now);

    // wave scheduler: 5 zone hits 60 ms apart — a pulse rolling up (or down)
    // the body in ~300 ms
    if (gWaveT >= 0.0f) {
        float prev = gWaveT;
        gWaveT += dt;
        for (int step = 0; step < NZONES; step++) {
            float fireAt = step * 0.06f;
            if (prev <= fireAt && gWaveT > fireAt) {
                int z = gWaveDir > 0 ? (NZONES - 1 - step) : step;
                midiStrikeZone(z, gWaveVel, gTuneHz[z]);
                gTestPulse[z] = 0.15f;
            }
        }
        if (gWaveT > NZONES * 0.06f + 0.2f) gWaveT = -1.0f;
    }

    // timed pulses (test buttons / slam / wave) release themselves
    for (int z = 0; z < NZONES; z++)
        if (gTestPulse[z] > 0.0f) {
            gTestPulse[z] -= dt;
            if (gTestPulse[z] <= 0.0f && !zoneHeldElsewhere(z))
                gEngine.params.padGate[z].store(0);
        }

    // debounced persistence of tuning + CC maps
    if (gTunerDirtyT > 0 && now - gTunerDirtyT > 1.0) {
        gTunerDirtyT = 0;
        saveTunerState();
    }
}

// ── TUNER panel ──
// drawn inline inside the ARTIFACT panel (TUNER sub-tab)
static void drawTunerBody() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    double now = glfwGetTime();
    bool anyDrone = false;
    for (int z = 0; z < NZONES; z++) anyDrone |= gTuneDrone[z];

    // ── row 1: tunings · LEARN · DRONE ALL / RELEASE · CLEAR MAPS ──
    for (int t = 0; t < kNumTunings; t++) {
        if (t) ImGui::SameLine();
        if (ImGui::SmallButton(kTunings[t].name)) {
            for (int z = 0; z < NZONES; z++) setZoneHz(z, kTunings[t].hz[z]);
            gTunerDirtyT = now;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", kTunings[t].note);
    }
    ImGui::SameLine(0, 26);
    {
        bool lit = gLearnArmed;
        if (lit) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.22f));
        const char* lbl = gLearnSel >= 0 ? "MOVE A KNOB..." : (gLearnArmed ? "LEARN: pick a cell" : "LEARN");
        if (ImGui::SmallButton(lbl)) {
            gLearnArmed = !gLearnArmed;
            gLearnSel = -1;
        }
        if (lit) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("arm, click an orb / HZ / LVL cell, then move a knob on your controller");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(anyDrone ? "RELEASE" : "DRONE ALL"))
        for (int z = 0; z < NZONES; z++) setDrone(z, !anyDrone);

    // ── ruler canvas ──
    float w = ImGui::GetContentRegionAvail().x;
    ImVec2 o = ImGui::GetCursorScreenPos();
    const float canvasH = 176.0f;
    ImGui::InvisibleButton("##ruler", ImVec2(w, canvasH));
    bool canvasHover = ImGui::IsItemHovered();
    const float pad = 26.0f;
    float rx0 = o.x + pad, rx1 = o.x + w - pad, rw = rx1 - rx0;
    const float lnRange = std::log(kTuneMax / kTuneMin);
    auto hzX = [&](float hz) { return rx0 + rw * std::log(hz / kTuneMin) / lnRange; };
    auto xHz = [&](float x) { return kTuneMin * std::exp(lnRange * (x - rx0) / rw); };
    float ry = o.y + 58.0f;   // ruler line
    float oy = ry + 60.0f;    // orb centre line
    const float orbR = 15.0f;

    // live energy: a bump at each zone's frequency, height = its meter
    {
        const int P = 140;
        ImVec2 pts[P];
        for (int i = 0; i < P; i++) {
            float x = rx0 + rw * i / (P - 1);
            float e = 0;
            for (int z = 0; z < NZONES; z++) {
                float d = (x - hzX(gTuneHz[z])) / 15.0f;
                e += gEngine.meter[2 + z].load() * std::exp(-d * d);
            }
            pts[i] = ImVec2(x, ry - 7.0f - 42.0f * std::min(1.0f, e));
        }
        dl->AddPolyline(pts, P, W(0.32f), 0, 1.2f);
    }

    // shade what the ring transducers cannot reproduce (above 80 Hz); only the
    // centre ButtKicker reaches up there
    {
        float x80 = hzX(80.0f);
        dl->AddRectFilled(ImVec2(x80, ry - 52), ImVec2(rx1, oy + orbR + 26), IM_COL32(255, 255, 255, 6), 4);
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        dl->AddText(ImVec2(x80 + 6, ry - 50), W(0.22f), "centre only");
        if (gFontSmall) ImGui::PopFont();
    }
    // ruler + ticks (log scale, labels on the round numbers)
    dl->AddLine(ImVec2(rx0, ry), ImVec2(rx1, ry), W(0.22f), 1.0f);
    static const int ticks[] = {20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80, 90,
                                100, 120, 140, 150, 160, 180, 200};
    static const int labels[] = {20, 25, 30, 40, 50, 60, 80, 100, 120, 150, 200};
    if (gFontSmall) ImGui::PushFont(gFontSmall);
    for (int t : ticks) {
        bool lab = false;
        for (int l : labels) lab |= (l == t);
        float x = hzX((float)t);
        dl->AddLine(ImVec2(x, ry), ImVec2(x, ry + (lab ? 8.0f : 4.0f)), W(lab ? 0.5f : 0.22f), 1.0f);
        if (lab) {
            char b[8];
            snprintf(b, sizeof(b), "%d", t);
            ImVec2 ts = ImGui::CalcTextSize(b);
            dl->AddText(ImVec2(x - ts.x / 2, ry + 11), W(0.42f), b);
        }
    }
    if (gFontSmall) ImGui::PopFont();

    // hover: nearest orb within reach (only while nothing else is being dragged)
    int hoverZ = -1;
    if (canvasHover && gTuneDrag < 0) {
        float best = 20.0f;
        for (int z = 0; z < NZONES; z++) {
            float dx = io.MousePos.x - hzX(gTuneHz[z]), dy = io.MousePos.y - oy;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d < best) { best = d; hoverZ = z; }
        }
    }
    if (hoverZ >= 0 || gTuneDrag >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (hoverZ >= 0 && ImGui::IsMouseClicked(0)) {
        gTuneDrag = hoverZ;
        gTuneDragHz0 = gTuneHz[hoverZ];
        gTuneDragX0 = io.MousePos.x;
        gTuneDragShift = io.KeyShift;
        if (gLearnArmed) gLearnSel = hoverZ;
        if (!gTuneDrone[hoverZ]) { // audition while held
            gTuneAudition = true;
            armZoneVoice(hoverZ, PAD_PURE);
            gEngine.params.padHz[hoverZ].store(gTuneHz[hoverZ]);
            gEngine.params.padVel[hoverZ].store(0.9f);
            gEngine.params.padGate[hoverZ].store(1);
        }
    }
    if (hoverZ >= 0 && ImGui::IsMouseClicked(1)) setDrone(hoverZ, !gTuneDrone[hoverZ]);
    if (gTuneDrag >= 0) {
        if (ImGui::IsMouseDown(0)) {
            if (io.KeyShift != gTuneDragShift) { // re-anchor when shift toggles mid-drag
                gTuneDragShift = io.KeyShift;
                gTuneDragHz0 = gTuneHz[gTuneDrag];
                gTuneDragX0 = io.MousePos.x;
            }
            float hz = io.KeyShift
                ? gTuneDragHz0 * std::exp(lnRange * (io.MousePos.x - gTuneDragX0) / rw * 0.08f)
                : xHz(io.MousePos.x);
            setZoneHz(gTuneDrag, hz);
        } else {
            int z = gTuneDrag;
            gTuneDrag = -1;
            if (gTuneAudition && !zoneHeldElsewhere(z)) gEngine.params.padGate[z].store(0);
            gTuneAudition = false;
            gTunerDirtyT = now;
        }
    }

    // beat readouts between frequency-neighbours: the throb rate you feel
    // where two zones overlap on the body
    {
        int ord[NZONES];
        for (int i = 0; i < NZONES; i++) ord[i] = i;
        std::sort(ord, ord + NZONES, [](int a, int b) { return gTuneHz[a] < gTuneHz[b]; });
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        for (int i = 0; i + 1 < NZONES; i++) {
            float a = gTuneHz[ord[i]], b = gTuneHz[ord[i + 1]];
            float d = b - a;
            if (d < 0.05f || d > 24.0f) continue;
            char t[16];
            snprintf(t, sizeof(t), "%.1f", d);
            ImVec2 ts = ImGui::CalcTextSize(t);
            float mx = (hzX(a) + hzX(b)) / 2;
            dl->AddText(ImVec2(mx - ts.x / 2, oy + orbR + 10), W(0.45f), t);
        }
        if (gFontSmall) ImGui::PopFont();
    }

    // orbs on stems, FEET drawn first (1) … HEAD last (5)
    for (int i = 0; i < NZONES; i++) {
        int z = kTuneOrder[i];
        float x = hzX(gTuneHz[z]);
        float lvl = gEngine.meter[2 + z].load();
        bool drone = gTuneDrone[z], hov = hoverZ == z || gTuneDrag == z;
        dl->AddLine(ImVec2(x, ry), ImVec2(x, oy - orbR), W(0.22f + 0.5f * lvl), 1.0f);
        if (lvl > 0.04f) dl->AddCircleFilled(ImVec2(x, oy), orbR + 4 + 10 * lvl, W(0.16f * lvl));
        float a = drone ? 0.95f : 0.30f + 0.55f * lvl;
        if (hov) a = std::min(1.0f, a + 0.18f);
        dl->AddCircleFilled(ImVec2(x, oy), orbR, W(a));
        if (drone) dl->AddCircle(ImVec2(x, oy), orbR + 4.0f, W(0.85f), 0, 1.6f);
        if (gLearnSel >= 0 && gLearnSel % NZONES == z) {
            float pulse = 0.5f + 0.5f * std::sin((float)now * 6.0f);
            dl->AddCircle(ImVec2(x, oy), orbR + 8.0f, W(0.25f + 0.5f * pulse), 0, 2.0f);
        }
        char num[4];
        snprintf(num, sizeof(num), "%d", i + 1);
        ImVec2 ts = ImGui::CalcTextSize(num);
        dl->AddText(ImVec2(x - ts.x / 2, oy - ts.y / 2),
                    a > 0.55f ? IM_COL32(0, 0, 0, 225) : W(0.92f), num);
    }

    // ── columns: name · Hz · note · DRONE · CC cells ──
    ImVec2 t0 = ImGui::GetCursorScreenPos();
    const float tableH = 92.0f;
    float colW = w / NZONES;
    for (int i = 0; i < NZONES; i++) {
        int z = kTuneOrder[i];
        float cx = t0.x + colW * i;
        if (i) dl->AddLine(ImVec2(cx - 8, t0.y + 2), ImVec2(cx - 8, t0.y + tableH - 8), W(0.07f), 1.0f);
        char buf[48];
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        snprintf(buf, sizeof(buf), "%d  %s", i + 1, kZoneNames[z]);
        dl->AddText(ImVec2(cx, t0.y), W(0.38f), buf);
        if (gFontSmall) ImGui::PopFont();
        bool hzFlash = now - gCcFlashT[z][0] < 0.15;
        snprintf(buf, sizeof(buf), "%.2f", gTuneHz[z]);
        dl->AddText(ImVec2(cx, t0.y + 16), W(hzFlash ? 1.0f : 0.92f), buf);
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        ImVec2 hs = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(cx + hs.x * 1.25f + 6, t0.y + 20), W(0.35f), "Hz");
        hzToNote(gTuneHz[z], buf, sizeof(buf));
        dl->AddText(ImVec2(cx, t0.y + 38), W(0.5f), buf);
        if (gFontSmall) ImGui::PopFont();

        // DRONE toggle
        ImGui::SetCursorScreenPos(ImVec2(cx, t0.y + 58));
        ImGui::PushID(z);
        bool litD = gTuneDrone[z];
        if (litD) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.28f));
        if (ImGui::SmallButton("DRONE")) setDrone(z, !gTuneDrone[z]);
        if (litD) ImGui::PopStyleColor();

        // CC cells: in LEARN mode they arm, otherwise they show the binding
        ImGui::SameLine(0, 6);
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        int ccHz = -1, ccLv = -1;
        for (auto& m : gCcMaps)
            if (m.zone == z) (m.target == 0 ? ccHz : ccLv) = m.cc;
        if (gLearnArmed) {
            bool armHz = gLearnSel == z, armLv = gLearnSel == z + NZONES;
            if (armHz) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.3f));
            if (ImGui::SmallButton("HZ")) gLearnSel = z;
            if (armHz) ImGui::PopStyleColor();
            ImGui::SameLine(0, 3);
            if (armLv) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.3f));
            if (ImGui::SmallButton("LVL")) gLearnSel = z + NZONES;
            if (armLv) ImGui::PopStyleColor();
        } else {
            ImVec2 cp = ImGui::GetCursorScreenPos();
            bool lvFlash = now - gCcFlashT[z][1] < 0.15;
            if (ccHz >= 0) snprintf(buf, sizeof(buf), "CC%d", ccHz); else snprintf(buf, sizeof(buf), "%s", "");
            dl->AddText(ImVec2(cp.x, cp.y + 4), W(hzFlash ? 0.95f : 0.45f), buf);
            if (ccLv >= 0) {
                snprintf(buf, sizeof(buf), "L CC%d", ccLv);
                dl->AddText(ImVec2(cp.x, cp.y + 17), W(lvFlash ? 0.95f : 0.45f), buf);
            }
            ImGui::Dummy(ImVec2(60, 10));
        }
        if (gFontSmall) ImGui::PopFont();
        ImGui::PopID();
    }
    ImGui::SetCursorScreenPos(ImVec2(t0.x, t0.y + tableH));

    if (gFontSmall) ImGui::PushFont(gFontSmall);
    ImGui::TextDisabled("drag orb = tune  ·  shift = fine  ·  right-click or keys 1-5 = drone  ·  esc = release  ·  number between orbs = beat Hz");
    if (gFontSmall) ImGui::PopFont();

    // keys 1–5 toggle drones while the tuner is showing
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        for (int i = 0; i < NZONES; i++)
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i), false))
                setDrone(kTuneOrder[i], !gTuneDrone[kTuneOrder[i]]);
}

// drawn inline inside the ARTIFACT panel (MIDI sub-tab): controller in, octagon out
static void drawMidiBody() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    double now = glfwGetTime();
    float w = ImGui::GetContentRegionAvail().x;
    auto light = [&](ImU32 col) {
        ImVec2 cp = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(cp.x + 5, cp.y + 10), 4.0f, col);
        ImGui::Dummy(ImVec2(14, 0));
        ImGui::SameLine();
    };
    // ── IN: the controller ──
    if (gFontSmall) ImGui::PushFont(gFontSmall);
    ImGui::TextDisabled("IN");
    if (gFontSmall) ImGui::PopFont();
#ifdef __APPLE__
    {
        bool conn = gMidi.connected();
        bool flash = now - gMidiActFlashT < 0.12;
        light(flash ? IM_COL32(255, 255, 255, 255) : conn ? IM_COL32(80, 220, 120, 200) : W(0.15f));
        if (conn) ImGui::Text("%s", gMidi.deviceName().c_str());
        else ImGui::TextDisabled("no controller");
    }
#else
    ImGui::TextDisabled("macOS only in this build");
#endif
    if (gFontSmall) ImGui::PushFont(gFontSmall);
    ImGui::TextDisabled("pads 36-40 play the rings  ·  48-59 VFX  ·  60-67 tuner VFX");
    if (gFontSmall) ImGui::PopFont();
    if (!gCcMaps.empty()) {
        ImGui::Dummy(ImVec2(0, 2));
        for (auto& m : gCcMaps) {
            if (gFontSmall) ImGui::PushFont(gFontSmall);
            ImGui::TextDisabled("CC %d  ->  %s %s", m.cc, kZoneNames[m.zone], m.target == 0 ? "Hz" : "level");
            if (gFontSmall) ImGui::PopFont();
        }
        if (ImGui::SmallButton("CLEAR MAPS")) { gCcMaps.clear(); saveTunerState(); }
    }

    // ── OUT: the octagon as a controller ──
    ImGui::Dummy(ImVec2(0, 16));
    {
        ImVec2 sp = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(sp.x, sp.y), ImVec2(sp.x + w, sp.y), W(0.08f), 1.0f);
    }
    ImGui::Dummy(ImVec2(0, 10));
#ifdef __APPLE__
    bool portOk = gMidiOut.running();
#else
    bool portOk = false;
#endif
    if (gFontSmall) ImGui::PushFont(gFontSmall);
    ImGui::TextDisabled("OUT   ·   \"AVA OS Octagon\"%s", portOk ? "" : "   ·   port unavailable");
    if (gFontSmall) ImGui::PopFont();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("rings = octaves, slices = scale steps, rim = velocity, angle = bend, hold = pressure\nzone sliders CC 20-24 · orbs CC 40-44 · Intensity..Void CC 30-38 · master CC 7");
    ImGui::PushID("midiout");
    {
        bool oflash = now - gOutFlashT < 0.12;
        light(oflash ? IM_COL32(255, 255, 255, 255) : (gOutOn && portOk ? IM_COL32(80, 220, 120, 200) : W(0.15f)));
    }
    bool litO = gOutOn;
    if (litO) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 0.28f));
    if (ImGui::SmallButton(gOutOn ? "ON" : "OFF")) {
        if (gOutOn) outNoteOff();
        gOutOn = !gOutOn;
    }
    if (litO) ImGui::PopStyleColor();
    ImGui::SameLine(0, 14);
    if (ImGui::SmallButton("-")) { outNoteOff(); gOutRoot = std::max(12, gOutRoot - 1); }
    ImGui::SameLine(0, 4);
    ImGui::Text("root %s%d", kKeyNames[gOutRoot % 12], gOutRoot / 12 - 1);
    ImGui::SameLine(0, 4);
    if (ImGui::SmallButton("+")) { outNoteOff(); gOutRoot = std::min(72, gOutRoot + 1); }
    ImGui::SameLine(0, 14);
    if (ImGui::SmallButton(gOutMinor ? "MINOR" : "MAJOR")) { outNoteOff(); gOutMinor = !gOutMinor; }
    ImGui::SameLine(0, 14);
    if (ImGui::SmallButton(gOutZoneCh ? "ZONE CHANNELS" : "CHANNEL 1")) { outNoteOff(); gOutZoneCh = !gOutZoneCh; }
    ImGui::PopID();
    if (gOutLast[0]) {
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        ImGui::TextDisabled("%s", gOutLast);
        if (gFontSmall) ImGui::PopFont();
    }
}

// VISUAL panel: shader library, its parameters, and the projector output
static void drawVisualBody(GLFWwindow* win) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 9));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 7));
    auto sectionHeader = [](const char* t, bool first = false) {
        if (!first) ImGui::Dummy(ImVec2(0, 8));
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        ImGui::TextDisabled("%s", t);
        if (gFontSmall) ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 1));
    };
    {
        sectionHeader("SHADER", true);
        static char filter[64] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##f", "search shaders", filter, sizeof(filter));
        ImGui::BeginChild("##list", ImVec2(-1, 300));
        if (ImGui::Selectable("None (black)", !gShaders.active())) gShaders.unload();
        auto lower = [](std::string s) {
            for (auto& c : s) c = (char)tolower(c);
            return s;
        };
        std::string f = lower(filter);
        for (int i = 0; i < (int)gShaders.entries().size(); i++) {
            const auto& e = gShaders.entries()[i];
            if (!f.empty() && lower(e.title).find(f) == std::string::npos) continue;
            if (ImGui::Selectable(e.title.c_str(), i == gShaders.currentIndex())) {
                if (!gShaders.load(i))
                    printf("shader load failed [%s]: %s\n", e.title.c_str(),
                           gShaders.lastError.c_str());
            }
        }
        ImGui::EndChild();
        sectionHeader("PARAMETERS");
        if (!gShaders.active()) {
            ImGui::TextDisabled("No shader loaded");
        } else {
            ImGui::BeginChild("##params", ImVec2(-1, 250));
            std::string lastGroup = "\x01";
            auto rowLabel = [](const std::string& l) {
                ImGui::TextColored(ImVec4(1, 1, 1, 0.72f), "%s", l.c_str());
            };
            for (auto& p : gShaders.params()) {
                if (p.group != lastGroup) {
                    lastGroup = p.group;
                    if (!p.group.empty()) {
                        ImGui::Dummy(ImVec2(0, 6));
                        if (gFontSmall) ImGui::PushFont(gFontSmall);
                        ImGui::TextDisabled("%s", p.group.c_str());
                        if (gFontSmall) ImGui::PopFont();
                    }
                }
                ImGui::PushID(p.name.c_str());
                switch (p.type) {
                    case ShaderParam::Float:
                    case ShaderParam::Event:
                        rowLabel(p.label);
                        ImGui::SetNextItemWidth(-1);
                        ImGui::SliderFloat("##v", &p.cur[0], p.minV, p.maxV, "%.2f");
                        break;
                    case ShaderParam::Bool: {
                        bool b = p.cur[0] > 0.5f;
                        if (ImGui::Checkbox(p.label.c_str(), &b)) p.cur[0] = b ? 1.f : 0.f;
                        break;
                    }
                    case ShaderParam::Long: {
                        rowLabel(p.label);
                        int cur = (int)p.cur[0];
                        std::string preview = std::to_string(cur);
                        for (size_t k = 0; k < p.values.size(); k++)
                            if (p.values[k] == cur && k < p.labels.size()) preview = p.labels[k];
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::BeginCombo("##c", preview.c_str())) {
                            for (size_t k = 0; k < p.values.size(); k++) {
                                std::string l = k < p.labels.size() ? p.labels[k]
                                                : std::to_string(p.values[k]);
                                if (ImGui::Selectable(l.c_str(), p.values[k] == cur))
                                    p.cur[0] = (float)p.values[k];
                            }
                            ImGui::EndCombo();
                        }
                        break;
                    }
                    case ShaderParam::Color:
                        rowLabel(p.label);
                        ImGui::SetNextItemWidth(-1);
                        ImGui::ColorEdit4("##col", p.cur, ImGuiColorEditFlags_Float);
                        break;
                    case ShaderParam::Point2D:
                        rowLabel(p.label);
                        ImGui::SetNextItemWidth(-1);
                        ImGui::DragFloat2("##pt", p.cur, 0.005f);
                        break;
                    case ShaderParam::Image:
                        break; // not fed — hidden rather than noise
                    case ShaderParam::Text: {
                        rowLabel(p.label);
                        char buf[128];
                        snprintf(buf, sizeof(buf), "%s", p.text.c_str());
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::InputText("##t", buf, sizeof(buf)))
                            p.text = buf;
                        break;
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::Dummy(ImVec2(0, 2));
            if (ImGui::Button("Reset defaults"))
                for (auto& p : gShaders.params())
                    for (int k = 0; k < 4; k++) p.cur[k] = p.def[k];
        }
        sectionHeader("DISPLAY");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##edgefade", &gEdgeFade, 0.0f, 1.0f,
                           "Edge fade  %.2f");
        if (gExtWin && ImGui::Selectable("Close external output")) {
            glfwDestroyWindow(gExtWin);
            gExtWin = nullptr;
            glfwMakeContextCurrent(win);
        }
        int mcount = 0;
        GLFWmonitor** mons = glfwGetMonitors(&mcount);
        GLFWmonitor* mainMon = glfwGetPrimaryMonitor();
        for (int m = 0; m < mcount; m++) {
            const GLFWvidmode* mode = glfwGetVideoMode(mons[m]);
            char row[160];
            snprintf(row, sizeof(row), "%s  ·  %dx%d%s", glfwGetMonitorName(mons[m]),
                     mode->width, mode->height,
                     mons[m] == mainMon ? "  (main)" : "");
            if (ImGui::Selectable(row)) {
                if (gExtWin) { glfwDestroyWindow(gExtWin); gExtWin = nullptr; }
                glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
                glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
                glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
                glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
                glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
                gExtWin = glfwCreateWindow(mode->width, mode->height,
                                           "AVA Output", mons[m], win);
                if (gExtWin) {
                    glfwMakeContextCurrent(gExtWin);
                    glfwSwapInterval(0); // avoid double-vsync stall
                    glfwMakeContextCurrent(win);
                }
            }
        }
    }
    ImGui::PopStyleVar(2);
}

// ── icon glyphs: small monochrome vectors, one per voice / preset ──
static void drawIcon(ImDrawList* d, int icon, ImVec2 c, float s, ImU32 col) {
    const float th = std::max(1.5f, s * 0.085f);
    auto P = [&](float x, float y) { return ImVec2(c.x + x * s * 0.5f, c.y + y * s * 0.5f); };
    auto wave = [&](float cycles, float amp, float y0, float x0 = -1.0f, float x1 = 1.0f) {
        ImVec2 pts[24];
        for (int i = 0; i < 24; i++) {
            float t = (float)i / 23;
            pts[i] = P(x0 + (x1 - x0) * t, y0 + amp * std::sin(2 * dsp::kPi * cycles * t));
        }
        d->AddPolyline(pts, 24, col, 0, th);
    };
    auto line = [&](float x0, float y0, float x1, float y1) { d->AddLine(P(x0, y0), P(x1, y1), col, th); };
    auto chevron = [&](float y, float dir) { // dir 1 = up
        line(-0.6f, y + 0.35f * dir, 0.0f, y - 0.35f * dir);
        line(0.0f, y - 0.35f * dir, 0.6f, y + 0.35f * dir);
    };
    switch (icon) {
        case IC_SINE: wave(1.0f, 0.55f, 0); break;
        case IC_WHALE: wave(0.5f, 0.7f, 0.1f); break;
        case IC_ZIGZAG: {
            ImVec2 pts[7];
            for (int i = 0; i < 7; i++) pts[i] = P(-0.9f + 0.3f * i, (i & 1) ? -0.55f : 0.55f);
            d->AddPolyline(pts, 7, col, 0, th); break;
        }
        case IC_HEART: {
            ImVec2 pts[32];
            for (int i = 0; i < 32; i++) {
                float t = 2 * dsp::kPi * i / 32;
                float x = 16 * std::pow(std::sin(t), 3.0f);
                float y = -(13 * std::cos(t) - 5 * std::cos(2 * t) - 2 * std::cos(3 * t) - std::cos(4 * t));
                pts[i] = P(x / 17.0f * 0.85f, y / 17.0f * 0.85f + 0.05f);
            }
            d->AddConvexPolyFilled(pts, 32, col); break;
        }
        case IC_TREMOLO: wave(4.0f, 0.4f, 0); break;
        case IC_DROP: {
            ImVec2 pts[12];
            for (int i = 0; i < 12; i++) { float t = i / 11.0f; pts[i] = P(-0.8f + 1.6f * t, -0.6f + 1.2f * t * t); }
            d->AddPolyline(pts, 12, col, 0, th);
            line(0.8f, 0.6f, 0.4f, 0.55f); line(0.8f, 0.6f, 0.7f, 0.2f); break;
        }
        case IC_RISER: line(-0.8f, 0.6f, 0.8f, -0.6f); line(0.8f, -0.6f, 0.35f, -0.6f); line(0.8f, -0.6f, 0.8f, -0.15f); break;
        case IC_HORN: {
            ImVec2 pts[4] = {P(-0.8f, -0.25f), P(0.7f, -0.75f), P(0.7f, 0.75f), P(-0.8f, 0.25f)};
            d->AddPolyline(pts, 4, col, ImDrawFlags_Closed, th); line(-0.8f, -0.25f, -0.8f, 0.25f); break;
        }
        case IC_BELL: {
            ImVec2 pts[24];
            for (int i = 0; i < 24; i++) { float x = -1.0f + 2.0f * i / 23; pts[i] = P(x * 0.9f, 0.55f - 1.15f * std::exp(-x * x * 5.0f)); }
            d->AddPolyline(pts, 24, col, 0, th); break;
        }
        case IC_BURST:
            for (int i = 0; i < 8; i++) { float a = i * dsp::kPi / 4; line(0.25f * std::cos(a), 0.25f * std::sin(a), 0.85f * std::cos(a), 0.85f * std::sin(a)); }
            break;
        case IC_BOLT: {
            ImVec2 pts[6] = {P(0.25f, -0.9f), P(-0.45f, 0.1f), P(0.05f, 0.1f), P(-0.25f, 0.9f), P(0.45f, -0.15f), P(-0.05f, -0.15f)};
            d->AddPolyline(pts, 6, col, ImDrawFlags_Closed, th); break;
        }
        case IC_TENSION: wave(5.0f, 0.25f, 0.25f); line(-0.9f, 0.7f, 0.9f, -0.7f); break;
        case IC_KICK: d->AddCircle(P(0, 0), s * 0.42f, col, 0, th); d->AddCircleFilled(P(0, 0), s * 0.12f, col); break;
        case IC_TOM: d->AddCircle(P(-0.35f, 0.1f), s * 0.27f, col, 0, th); d->AddCircle(P(0.4f, -0.15f), s * 0.22f, col, 0, th); break;
        case IC_BOOM: d->AddCircleFilled(P(0, 0), s * 0.22f, col); d->AddCircle(P(0, 0), s * 0.44f, col, 0, th); break;
        case IC_SNARE: d->AddCircle(P(0, 0), s * 0.42f, col, 0, th); line(-0.6f, -0.35f, 0.6f, 0.35f); line(-0.6f, 0.35f, 0.6f, -0.35f); break;
        case IC_ROLL: for (int i = 0; i < 4; i++) line(-0.75f + 0.5f * i, 0.6f, -0.75f + 0.5f * i, -0.2f - 0.15f * i); break;
        case IC_CLAP: line(-0.7f, 0.6f, -0.1f, -0.3f); line(0.7f, 0.6f, 0.1f, -0.3f); line(0, -0.5f, 0, -0.9f); line(-0.45f, -0.55f, -0.65f, -0.85f); line(0.45f, -0.55f, 0.65f, -0.85f); break;
        case IC_UP: chevron(-0.25f, 1); chevron(0.4f, 1); break;
        case IC_DOWN: chevron(0.25f, -1); chevron(-0.4f, -1); break;
        case IC_RIPPLE: d->AddCircleFilled(P(0, 0), s * 0.09f, col); d->AddCircle(P(0, 0), s * 0.26f, col, 0, th); d->AddCircle(P(0, 0), s * 0.45f, col, 0, th); break;
        case IC_BOUNCE: chevron(-0.45f, 1); chevron(0.45f, -1); break;
        case IC_ECG: {
            ImVec2 pts[9] = {P(-0.95f, 0), P(-0.45f, 0), P(-0.3f, -0.35f), P(-0.15f, 0.75f), P(0.0f, -0.85f), P(0.15f, 0.3f), P(0.3f, 0), P(0.95f, 0), P(0.95f, 0)};
            d->AddPolyline(pts, 8, col, 0, th); break;
        }
        case IC_SWEEP: line(-0.5f, -0.8f, -0.5f, 0.6f); line(-0.5f, 0.6f, -0.85f, 0.25f); line(-0.5f, 0.6f, -0.15f, 0.25f);
                       line(0.5f, 0.8f, 0.5f, -0.6f); line(0.5f, -0.6f, 0.15f, -0.25f); line(0.5f, -0.6f, 0.85f, -0.25f); break;
        case IC_STAIRS: {
            ImVec2 pts[8] = {P(-0.9f, 0.8f), P(-0.45f, 0.8f), P(-0.45f, 0.25f), P(0.0f, 0.25f), P(0.0f, -0.3f), P(0.45f, -0.3f), P(0.45f, -0.85f), P(0.9f, -0.85f)};
            d->AddPolyline(pts, 8, col, 0, th); break;
        }
        case IC_RISEDROP: line(-0.9f, 0.6f, 0.35f, -0.7f); line(0.35f, -0.7f, 0.35f, 0.7f); line(0.35f, 0.7f, 0.9f, 0.7f); break;
        case IC_STORM: for (int i = 0; i < 5; i++) { float x = -0.8f + 0.4f * i; d->AddCircleFilled(P(x, (i & 1) ? -0.4f : 0.4f), s * 0.09f, col); } break;
        case IC_SLAM: for (int i = 0; i < 8; i++) { float a = i * dsp::kPi / 4; line(0.35f * std::cos(a), 0.35f * std::sin(a), 0.9f * std::cos(a), 0.9f * std::sin(a)); } d->AddCircleFilled(P(0, 0), s * 0.2f, col); break;
        default: d->AddCircle(P(0, 0), s * 0.4f, col, 0, th);
    }
}

// ARTIFACT panel, SOUNDS: one scrolling page of sections (no inner tabs).
// Each tile is icon + name + one short hint, clipped to its own bounds.
static void drawSoundsBody(float w) {
    buildVfx();
    double tnow = glfwGetTime();
    int cur = gEngine.params.padPatch.load();
    ImDrawList* pdl = ImGui::GetWindowDrawList();
    const float gapX = 8, gapY = 8, tileH = 60;
    const int cols = 3;
    const float tileW = std::floor((w - (cols - 1) * gapX) / (float)cols);
    int col = 0;

    auto section = [&](const char* title, const char* note, bool first) {
        if (col != 0) { ImGui::NewLine(); col = 0; }
        if (!first) ImGui::Dummy(ImVec2(0, 14));
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        ImGui::TextDisabled("%s", title);
        ImGui::SameLine(0, 10);
        ImGui::TextColored(ImVec4(1, 1, 1, 0.22f), "%s", note);
        if (gFontSmall) ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 2));
    };
    auto tile = [&](const char* id, int icon, const char* name, const char* hint, bool sel,
                    float progress, bool hot) -> bool {
        if (col > 0) ImGui::SameLine(0, gapX);
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1(p0.x + tileW, p0.y + tileH);
        bool pressed = ImGui::InvisibleButton(id, ImVec2(tileW, tileH));
        bool h = ImGui::IsItemHovered();
        pdl->AddRectFilled(p0, p1, sel ? IM_COL32(255, 255, 255, 30) : IM_COL32(255, 255, 255, h ? 16 : 8), 9);
        if (sel) pdl->AddRect(p0, p1, IM_COL32(255, 255, 255, 110), 9);
        if (progress > 0.0f)
            pdl->AddRectFilled(ImVec2(p0.x + 8, p1.y - 4), ImVec2(p0.x + 8 + (tileW - 16) * progress, p1.y - 2),
                               hot ? IM_COL32(255, 150, 70, 220) : IM_COL32(255, 255, 255, 160), 1);
        pdl->PushClipRect(p0, p1, true);
        drawIcon(pdl, icon, ImVec2(p0.x + 24, p0.y + tileH / 2), 22.0f, W(sel ? 0.95f : (h ? 0.85f : 0.6f)));
        pdl->AddText(ImVec2(p0.x + 46, p0.y + 12), W(sel ? 0.97f : 0.88f), name);
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        pdl->AddText(ImVec2(p0.x + 46, p0.y + 34), W(0.42f), hint);
        if (gFontSmall) ImGui::PopFont();
        pdl->PopClipRect();
        col = (col + 1) % cols;
        if (col == 0) ImGui::Dummy(ImVec2(0, gapY - 4));
        return pressed;
    };

    // header: what is armed, and a way to silence everything
    {
        ImVec2 h0 = ImGui::GetCursorScreenPos();
        char armed[64];
        snprintf(armed, sizeof(armed), "armed: %s", kPadPatches[std::min(std::max(cur, 0), NPATCHES - 1)].name);
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        ImGui::TextDisabled("%s", armed);
        if (gFontSmall) ImGui::PopFont();
        bool anyOn = !gSeq.empty();
        for (int z = 0; z < NZONES; z++) anyOn |= gEngine.params.padGate[z].load() != 0;
        ImGui::SameLine(w - 60);
        ImGui::SetCursorScreenPos(ImVec2(h0.x + w - 60, h0.y - 4));
        if (ImGui::InvisibleButton("##stopall", ImVec2(60, 22))) stopVfx();
        bool sh = ImGui::IsItemHovered();
        ImVec2 b0 = ImGui::GetItemRectMin();
        pdl->AddRectFilled(ImVec2(b0.x + 6, b0.y + 7), ImVec2(b0.x + 14, b0.y + 15), W(anyOn ? 0.9f : (sh ? 0.6f : 0.3f)), 1);
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        pdl->AddText(ImVec2(b0.x + 20, b0.y + 4), W(anyOn ? 0.9f : (sh ? 0.6f : 0.3f)), "STOP");
        if (gFontSmall) ImGui::PopFont();
        ImGui::SetCursorScreenPos(ImVec2(h0.x, h0.y + 24));
    }

    ImGui::BeginChild("##soundscroll", ImVec2(w, ImGui::GetContentRegionAvail().y - 6), false,
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar);
    pdl = ImGui::GetWindowDrawList();
    static const char* catTitle[NCATS] = {"PADS", "CINEMATIC", "DRUM"};
    static const char* catNote[NCATS] = {"rings + MIDI pads 36-40", "sweeps and hits", "one-shots"};
    for (int cat = 0; cat < NCATS; cat++) {
        section(catTitle[cat], catNote[cat], cat == 0);
        for (int p = 0; p < NPATCHES; p++) {
            if (kPadPatches[p].cat != cat) continue;
            char id[24]; snprintf(id, sizeof(id), "##snd%d", p);
            bool playing = false;
            for (int z = 0; z < NZONES; z++)
                if (gEngine.params.padPatchZ[z].load() == p && gEngine.params.padGate[z].load()) playing = true;
            if (tile(id, kPadPatches[p].icon, kPadPatches[p].name, kPadPatches[p].hint, p == cur, playing ? 1.0f : 0.0f, false)) {
                gEngine.params.padPatch.store(p);
                auditionVoice(p);
            }
        }
    }
    for (int pass = 0; pass < 2; pass++) {
        bool wantTuner = pass == 1;
        section(wantTuner ? "TUNER VFX" : "VFX", wantTuner ? "exaggerated · MIDI 60-67" : "whole body · MIDI 48-59", false);
        for (int i = 0; i < (int)gVfx.size(); i++) {
            if (gVfx[i].tuner != wantTuner) continue;
            char id[24]; snprintf(id, sizeof(id), "##vfx%d", i);
            float prog = 0.0f;
            if (gVfxLast == i) {
                float el = (float)(tnow - gVfxLastT);
                if (el < gVfx[i].length) prog = el / gVfx[i].length;
            }
            if (tile(id, gVfx[i].icon, gVfx[i].name, gVfx[i].hint, gVfxLast == i && prog > 0, prog, wantTuner))
                fireVfx(i);
        }
    }
    if (col != 0) ImGui::NewLine();
    ImGui::Dummy(ImVec2(0, 8));
    ImGui::EndChild();
}

// ── ARTIFACT mode: the octagon becomes a sound launcher ──
// Same glass geometry, but every slice carries one sound: the three inner
// rings are the voices (PADS, CINEMATIC, DRUM, each ring topped up with two
// matching presets), the outer ring is the VFX, the centre fires the armed
// voice through the whole body. Tap = play.
struct LaunchSlot { int kind; int idx; }; // kind 0 = voice, 1 = VFX (by name), -1 = empty
static int vfxIndex(const char* name) {
    buildVfx();
    for (int i = 0; i < (int)gVfx.size(); i++) if (std::strcmp(gVfx[i].name, name) == 0) return i;
    return -1;
}
static LaunchSlot launchSlot(int ring, int k) {
    static const int voices[3][6] = {
        {PAD_PURE, PAD_WHALE, PAD_QUAKE, PAD_HEART, PAD_PURR, PAD_DROP},
        {PAD_RISER, PAD_BRAAM, PAD_SWELL, PAD_IMPACT, PAD_THUNDER, PAD_TENSION},
        {PAD_KICK, PAD_TOM, PAD_BOOM, PAD_SNARE, PAD_ROLL, PAD_CLAP}};
    static const char* extra[3][2] = {
        {"HEARTBEAT", "WHALE SURGE"}, {"THUNDER ROLL", "BRAAM HIT"}, {"DRUM SWEEP", "CASCADE"}};
    static const char* outer[8] = {"WAVE UP", "WAVE DOWN", "RIPPLE", "BOUNCE", "EARTHQUAKE",
                                   "RISE & DROP", "FULL BODY SLAM", "TSUNAMI"};
    if (ring < 3) {
        if (k < 6) return {0, voices[ring][k]};
        return {1, vfxIndex(extra[ring][k - 6])};
    }
    return {1, vfxIndex(outer[k])};
}
static void drawOctagonLauncher(ImDrawList* dl, ImVec2 c, float R) {
    const int ringZone[4] = {ROOT, BELLY, HEART, HEAD};
    const float bounds[5] = {0.36f, 0.50f, 0.65f, 0.80f, 0.96f};
    float roundR = std::max(5.0f, R * 0.030f);
    double now = glfwGetTime();
    auto vert = [&](float r, int k) {
        float a = -dsp::kPi / 2 + dsp::kPi / 8 + k * dsp::kPi / 4;
        return ImVec2(c.x + r * R * std::cos(a), c.y + r * R * std::sin(a));
    };
    int hoverZone = -1, hoverK = 0;
    float hoverFrac = 0.5f;
    if (!ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive())
        hoverZone = octagonPadAt(c, R, ImGui::GetIO().MousePos, &hoverK, &hoverFrac);
    int hoverRing = -1;
    for (int r = 0; r < 4; r++) if (ringZone[r] == hoverZone) hoverRing = r;
    bool clicked = ImGui::IsMouseClicked(0);
    if (hoverZone >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    int cur = std::min(std::max(gEngine.params.padPatch.load(), 0), NPATCHES - 1);

    auto playing = [&](LaunchSlot sl) -> float { // 0 = idle, else brightness
        if (sl.kind == 0) {
            for (int z = 0; z < NZONES; z++)
                if (gEngine.params.padPatchZ[z].load() == sl.idx && gEngine.params.padGate[z].load()) return 1.0f;
            return 0.0f;
        }
        if (sl.kind == 1 && gVfxLast == sl.idx) {
            float el = (float)(now - gVfxLastT);
            if (el < gVfx[sl.idx].length) return 1.0f - 0.5f * el / gVfx[sl.idx].length;
        }
        return 0.0f;
    };

    for (int ring = 3; ring >= 0; ring--) {
        for (int k = 0; k < 8; k++) {
            LaunchSlot sl = launchSlot(ring, k);
            ImVec2 raw[4] = {vert(bounds[ring + 1], k), vert(bounds[ring + 1], k + 1),
                             vert(bounds[ring], k + 1), vert(bounds[ring], k)};
            ImVec2 q[4];
            insetConvexPoly(raw, 4, std::max(3.0f, R * 0.011f), q);
            ImVec2 ctr((q[0].x + q[1].x + q[2].x + q[3].x) / 4, (q[0].y + q[1].y + q[2].y + q[3].y) / 4);
            bool hov = hoverRing == ring && hoverK == k;
            float play = playing(sl);
            bool armed = sl.kind == 0 && sl.idx == cur;
            // quiet glass; voices a touch brighter than presets, armed voice outlined,
            // playing slices light up
            int a = sl.kind == 0 ? 40 : 26;
            if (hov) a += 30;
            a = (int)(a + play * 150);
            roundedPolyFill(dl, q, 4, roundR, IM_COL32(255, 255, 255, std::min(a, 240)),
                            armed ? IM_COL32(255, 255, 255, 190) : IM_COL32(255, 255, 255, 22),
                            armed ? 1.6f : 1.0f);
            int icon = sl.kind == 0 ? kPadPatches[sl.idx].icon : (sl.idx >= 0 ? gVfx[sl.idx].icon : -1);
            float isz = std::max(16.0f, R * 0.075f);
            ImU32 icol = play > 0.0f ? IM_COL32(0, 0, 0, 230) : W(hov || armed ? 0.95f : 0.72f);
            if (icon >= 0) drawIcon(dl, icon, ctr, isz, icol);
            if (hov) {
                const char* nm = sl.kind == 0 ? kPadPatches[sl.idx].name : gVfx[sl.idx].name;
                const char* ht = sl.kind == 0 ? kPadPatches[sl.idx].hint : gVfx[sl.idx].hint;
                ImGui::SetTooltip("%s\n%s", nm, ht);
                if (clicked) {
                    if (sl.kind == 0) { gEngine.params.padPatch.store(sl.idx); auditionVoice(sl.idx); }
                    else fireVfx(sl.idx);
                }
            }
        }
    }
    // centre: the armed voice through every ring
    {
        ImVec2 raw[8], pts[8];
        for (int k = 0; k < 8; k++) raw[k] = vert(bounds[0], k);
        insetConvexPoly(raw, 8, std::max(3.0f, R * 0.011f), pts);
        bool hov = hoverZone == FEET;
        bool play = false;
        for (int z = 0; z < NZONES; z++) play |= gEngine.params.padGate[z].load() != 0;
        int a = hov ? 235 : 205;
        roundedPolyFill(dl, pts, 8, roundR * 1.4f, IM_COL32(255, 255, 255, a),
                        IM_COL32(0, 0, 0, 26), 1.0f);
        drawIcon(dl, kPadPatches[cur].icon, c, std::max(22.0f, R * 0.12f), IM_COL32(0, 0, 0, play ? 255 : 200));
        if (gFontSmall) ImGui::PushFont(gFontSmall);
        ImVec2 ts = ImGui::CalcTextSize(kPadPatches[cur].name);
        dl->AddText(ImVec2(c.x - ts.x / 2, c.y + R * 0.09f), IM_COL32(0, 0, 0, 170), kPadPatches[cur].name);
        if (gFontSmall) ImGui::PopFont();
        if (hov) {
            ImGui::SetTooltip("%s through every ring", kPadPatches[cur].name);
            if (clicked) {
                for (int z = 0; z < NZONES; z++) {
                    midiStrikeZone(z, 1.0f, kPadPatches[cur].baseHz > 0 ? kPadPatches[cur].baseHz : gTuneHz[z], cur);
                    gTestPulse[z] = std::max(gTestPulse[z], kPadPatches[cur].cat == CAT_DRUM ? 0.3f : 1.5f);
                }
            }
        }
    }
}

static int runRouteTest(const char* devSub);
static int runShaderTest();
static int runPing(int argc, char** argv);
static int runPadTest();
static int runSoundTest();
static int runSplitTest(const char* wavPath);

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "--selftest") == 0) return runSelfTest();
    if (argc > 2 && std::strcmp(argv[1], "--routetest") == 0) return runRouteTest(argv[2]);
    if (argc > 2 && std::strcmp(argv[1], "--ping") == 0) return runPing(argc - 2, argv + 2);
    if (argc > 1 && std::strcmp(argv[1], "--padtest") == 0) return runPadTest();
    if (argc > 1 && std::strcmp(argv[1], "--soundtest") == 0) return runSoundTest();
    if (argc > 1 && std::strcmp(argv[1], "--splittest") == 0) return runSplitTest(argc > 2 ? argv[2] : nullptr);
    if (argc > 1 && std::strcmp(argv[1], "--shadertest") == 0) return runShaderTest();

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    GLFWwindow* win = glfwCreateWindow(1500, 860, "AVA OS", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glplatInit(); // no-op on macOS; loads GL entry points on Windows
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    static ImFontGlyphRangesBuilder grb;
    static ImVector<ImWchar> granges;
    grb.AddRanges(io.Fonts->GetGlyphRangesDefault());
    grb.AddRanges(io.Fonts->GetGlyphRangesGreek());
    grb.BuildRanges(&granges);
    ImFont* fontM = io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/Helvetica.ttc",
                                                 15.0f, nullptr, granges.Data);
    if (!fontM) io.Fonts->AddFontDefault();
    gFontSmall = io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/Helvetica.ttc",
                                              12.0f, nullptr, granges.Data);
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    ImGuiStyle& st = ImGui::GetStyle();
    st.WindowRounding = 0;
    st.Colors[ImGuiCol_WindowBg] = ImVec4(0.016f, 0.016f, 0.018f, 1.0f);
    st.Colors[ImGuiCol_PopupBg] = ImVec4(0.06f, 0.06f, 0.065f, 0.98f);
    st.Colors[ImGuiCol_Text] = ImVec4(1, 1, 1, 0.92f);
    st.Colors[ImGuiCol_Button] = ImVec4(1, 1, 1, 0.06f);
    st.Colors[ImGuiCol_ButtonHovered] = ImVec4(1, 1, 1, 0.12f);
    st.Colors[ImGuiCol_ButtonActive] = ImVec4(1, 1, 1, 0.20f);
    st.Colors[ImGuiCol_FrameBg] = ImVec4(1, 1, 1, 0.06f);
    st.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1, 1, 1, 0.10f);
    st.Colors[ImGuiCol_Header] = ImVec4(1, 1, 1, 0.10f);
    st.Colors[ImGuiCol_HeaderHovered] = ImVec4(1, 1, 1, 0.16f);
    st.Colors[ImGuiCol_SliderGrab] = ImVec4(1, 1, 1, 0.90f);
    st.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1, 1, 1, 1.0f);
    st.Colors[ImGuiCol_CheckMark] = ImVec4(1, 1, 1, 0.95f);
    st.Colors[ImGuiCol_FrameBgActive] = ImVec4(1, 1, 1, 0.14f);
    st.Colors[ImGuiCol_TextDisabled] = ImVec4(1, 1, 1, 0.32f);
    st.Colors[ImGuiCol_Separator] = ImVec4(1, 1, 1, 0.10f);
    st.Colors[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
    st.Colors[ImGuiCol_ScrollbarGrab] = ImVec4(1, 1, 1, 0.14f);
    st.Colors[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.06f, 0.065f, 1.0f);
    st.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
    st.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.06f, 0.06f, 0.065f, 1.0f);
    st.PopupRounding = 14;
    st.FrameRounding = 10;
    st.GrabRounding = 10;
    st.WindowPadding = ImVec2(14, 12);
    st.PopupBorderSize = 1.0f;
    st.Colors[ImGuiCol_Border] = ImVec4(1, 1, 1, 0.08f);

    gEngine.init();
    gRing.init(48000); // 1 s
    gDevices = listOutputDevices();
    // default: first device with >=7 out channels (the interface), else default
    for (int i = 0; i < (int)gDevices.size(); i++)
        if (gDevices[i].channels >= 7) { gSelDevice = i; break; }
    if (gSelDevice < 0 && !gDevices.empty()) gSelDevice = 0;
    applyPreset(kTabPreset[0]);
    // screenshot / automation hooks: start on a given mode + artifact sub-tab
    if (const char* m = getenv("AVA_MODE")) gMode = std::min(std::max(atoi(m), 0), 2);
    if (const char* t = getenv("AVA_ARTIFACT_TAB")) gArtifactTab = std::min(std::max(atoi(t), 0), 2);
    gTap.start(&gRing);
    startAudio(); // live on launch
#ifdef __APPLE__
    gMidi.start(); // any pad controller auto-connects (2 s hot-plug rescan)
    gMidiOut.start(); // publish the "AVA OS Octagon" virtual MIDI source
#endif
    for (auto& row : gOutCcLast) for (int& v : row) v = -1;
    loadTunerState();

    // shader background library — bundled copy first, dev checkout as fallback
    gShaders.loadLibrary(shaderLibraryDir());
    gShaderT0 = glfwGetTime();

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();

        // Escape: close the projector output first, else leave fullscreen
        {
            static bool escPrev = false;
            bool esc = glfwGetKey(win, GLFW_KEY_ESCAPE) == GLFW_PRESS ||
                       (gExtWin && glfwGetKey(gExtWin, GLFW_KEY_ESCAPE) == GLFW_PRESS);
            bool popupOpen = ImGui::GetCurrentContext() &&
                             ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId |
                                                        ImGuiPopupFlags_AnyPopupLevel);
            if (esc && !escPrev && !popupOpen) {
                bool anyDrone = false;
                for (int z = 0; z < NZONES; z++) anyDrone |= gTuneDrone[z];
                if (anyDrone) {
                    for (int z = 0; z < NZONES; z++) setDrone(z, false);
                } else if (gExtWin) {
                    glfwDestroyWindow(gExtWin);
                    gExtWin = nullptr;
                    glfwMakeContextCurrent(win);
                } else if (macWindowIsFullscreen(win)) {
                    macWindowExitFullscreen(win);
                }
            }
            escPrev = esc;
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        processMidi(glfwGetTime(), ImGui::GetIO().DeltaTime);
#ifdef __APPLE__
        { static double tIn = 0; double tn = glfwGetTime();
          if (gIn.running() && tn - tIn > 2.0) { tIn = tn; float pk = gIn.peak.load();
            if (pk > 0.01f) fprintf(stderr, "[input] peak %.2f\n", pk); } }
#endif

        // TEST ALL sweep: when the current zone's pulse ends, fire the next,
        // so each amp/transducer can be verified by feel in order
        if (gSweepZone >= 0 && gTestPulse[gSweepZone] <= 0.0f) {
            gSweepZone++;
            if (gSweepZone >= NZONES) gSweepZone = -1;
            else fireZonePulse(gSweepZone);
        }
        // remember when each interface channel last carried real signal
        {
            double tnow = glfwGetTime();
            for (int c = 0; c < OutputUnit::kMaxCh; c++)
                if (gOut.chanPeak[c].load() > 0.004f) gChanLastLive[c] = tnow;
        }

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->Pos);
        ImGui::SetNextWindowSize(vp->Size);
        ImGui::Begin("##root", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar |
                     ImGuiWindowFlags_NoBackground);
        if (gShaders.active())
            ImGui::GetBackgroundDrawList()->AddRectFilled(
                vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y),
                IM_COL32(0, 0, 0, 118));
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float W_ = vp->Size.x, H_ = vp->Size.y;

        // ── header: logo dot + name ──
        dl->AddCircleFilled(ImVec2(42, 40), 10, W(0.18f));
        dl->AddCircleFilled(ImVec2(42, 40), 6, W(0.95f));
        dl->AddText(ImVec2(60, 32), W(0.95f), "AVA OS");

        if (gShowHealth) {
            double tnow = glfwGetTime();
            // presence check every 2 s: is the selected interface still on USB?
            if (tnow - gHealthScanT > 2.0) {
                gHealthScanT = tnow;
                gDevPresent = false;
                if (gSelDevice >= 0 && gSelDevice < (int)gDevices.size())
                    for (auto& d : listOutputDevices())
                        if (d.id == gDevices[gSelDevice].id) { gDevPresent = true; break; }
            }
            ImGui::SetNextWindowPos(ImVec2(W_ - 542, 66), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(520, 0));
            if (ImGui::Begin("SYSTEM HEALTH", &gShowHealth,
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
                // keys 1-5 isolate a ring (FEET…HEAD), 0 restores all
                if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
                    for (int i = 0; i < NZONES; i++)
                        if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i), false))
                            setSoloZone(kTuneOrder[i]);
                    if (ImGui::IsKeyPressed(ImGuiKey_0, false)) setSoloZone(-1);
                }
                ImDrawList* hdl = ImGui::GetWindowDrawList();
                bool devOk = gSelDevice >= 0 && gSelDevice < (int)gDevices.size();
                ImGui::TextDisabled("INTERFACE");
                ImGui::Separator();
                if (!devOk) {
                    ImGui::TextColored(ImVec4(1, 0.35f, 0.3f, 1), "NO OUTPUT DEVICE SELECTED");
                } else {
                    OutDevice& dev = gDevices[gSelDevice];
                    ImGui::Text("%s  ·  %d out  ·  %.0fk", dev.name.c_str(), dev.channels,
                                dev.sampleRate / 1000.0);
                    if (!gDevPresent)
                        ImGui::TextColored(ImVec4(1, 0.35f, 0.3f, 1), "DISCONNECTED — check USB");
                    else if (!gOut.running())
                        ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "ENGINE STOPPED  %s",
                                           gOut.lastError.c_str());
                    else
                        ImGui::TextColored(ImVec4(0.4f, 1, 0.5f, 1), "RUNNING");
                    float hw = deviceHwVolume(dev.id);
                    if (hw >= 0) {
                        ImGui::Text("Interface hardware volume: %.0f%%%s", hw * 100.0f,
                                    hw >= 0.999f ? "   (MAX)" : "");
                        if (hw < 0.999f) {
                            ImGui::SameLine();
                            if (ImGui::SmallButton("MAX HW")) maxDeviceHwVolume(dev.id);
                        }
                    }
                }
                // software chain verdict
                {
                    float mv = gEngine.params.masterVolume.load();
                    float mg = gEngine.params.musicGain.load();
                    float mnT = 2.0f;
                    for (int z = 0; z < NZONES; z++)
                        mnT = std::min(mnT, gEngine.params.zoneTrim[z].load());
                    if (mv >= 0.999f && mg >= 1.499f && mnT >= 1.499f)
                        ImGui::TextColored(ImVec4(0.4f, 1, 0.5f, 1),
                                           "ALL SOFTWARE LEVELS AT MAX");
                    else
                        ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1),
                                           "below max — master %.2f · music %.2f · min trim %.2f",
                                           mv, mg, mnT);
                }
                ImGui::Spacing();
                if (ImGui::Button("MAX ALL LEVELS", ImVec2(160, 30))) maxAllLevels();
                ImGui::SameLine();
                bool sweeping = gSweepZone >= 0;
                if (ImGui::Button(sweeping ? "SWEEPING..." : "TEST ALL ZONES", ImVec2(160, 30)) &&
                    !sweeping) {
                    gSweepZone = 0;
                    fireZonePulse(0);
                }
                if (sweeping) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.55f, 0.92f, 1, 1), "-> %s",
                                       kZoneNames[gSweepZone]);
                }

                // per interface channel: what leaves the box toward each amp
                ImGui::Spacing();
                ImGui::TextDisabled("INTERFACE CHANNELS   ·   signal leaving each output");
                ImGui::Separator();
                int nch = gOut.running()
                              ? gOut.activeChannels
                              : (devOk ? std::min(gDevices[gSelDevice].channels,
                                                  (int)OutputUnit::kMaxCh)
                                       : 0);
                int musicL = gEngine.params.musicChanL.load();
                int surrL = gEngine.params.surrChanL.load();
                for (int c = 0; c < nch && c < OutputUnit::kMaxCh; c++) {
                    char lbl[64] = "";
                    auto app = [&](const char* s) { // portable append (no strlcat on mingw)
                        size_t n = strlen(lbl);
                        snprintf(lbl + n, sizeof(lbl) - n, "%s%s", lbl[0] ? " + " : "", s);
                    };
                    if (musicL >= 0 && c == musicL) app("MUSIC L");
                    if (musicL >= 0 && c == musicL + 1) app("MUSIC R");
                    if (surrL >= 0 && c == surrL) app("SURR L");
                    if (surrL >= 0 && c == surrL + 1) app("SURR R");
                    for (int z = 0; z < NZONES; z++)
                        if (gEngine.params.zoneChan[z].load() == c ||
                            gEngine.params.zoneChan2[z].load() == c) app(kZoneNames[z]);
                    bool assigned = lbl[0] != 0;
                    float pk = gOut.chanPeak[c].load();
                    bool live = gOut.running() && (tnow - gChanLastLive[c]) < 1.5;
                    ImVec2 cp = ImGui::GetCursorScreenPos();
                    ImU32 dot = !assigned ? IM_COL32(255, 255, 255, 46)
                                : live    ? IM_COL32(90, 255, 120, 255)
                                          : IM_COL32(255, 190, 60, 255);
                    hdl->AddCircleFilled(ImVec2(cp.x + 7, cp.y + 9), 4.5f, dot);
                    ImGui::Dummy(ImVec2(17, 0));
                    ImGui::SameLine();
                    ImGui::Text("Ch %-2d", c + 1);
                    ImGui::SameLine(78);
                    ImGui::TextColored(assigned ? ImVec4(1, 1, 1, 0.9f)
                                                : ImVec4(1, 1, 1, 0.3f),
                                       "%s", assigned ? lbl : "—");
                    ImGui::SameLine(238);
                    ImVec2 mp = ImGui::GetCursorScreenPos();
                    float mw = 140, mh = 8;
                    hdl->AddRectFilled(ImVec2(mp.x, mp.y + 5), ImVec2(mp.x + mw, mp.y + 5 + mh),
                                       IM_COL32(255, 255, 255, 18), 4);
                    float v = std::min(1.0f, pk);
                    if (v > 0.003f)
                        hdl->AddRectFilled(
                            ImVec2(mp.x, mp.y + 5), ImVec2(mp.x + mw * v, mp.y + 5 + mh),
                            pk > 0.98f ? IM_COL32(255, 120, 90, 255)
                                       : IM_COL32(140, 235, 255, 230),
                            4);
                    ImGui::Dummy(ImVec2(mw + 8, 0));
                    ImGui::SameLine();
                    if (pk > 0.0005f)
                        ImGui::TextDisabled("%5.1f dB", 20.0f * log10f(pk));
                    else
                        ImGui::TextDisabled("    —");
                    // thermal guard: heat as fraction of the RMS budget
                    {
                        float heat = gOut.chanHeat[c].load();
                        float tg = gOut.chanThermGain[c].load();
                        ImGui::SameLine(388);
                        if (heat > 0.001f) {
                            ImVec4 col = tg < 0.98f ? ImVec4(1, 0.35f, 0.3f, 1)
                                       : heat > 0.7f ? ImVec4(1, 0.75f, 0.3f, 1)
                                                     : ImVec4(1, 1, 1, 0.45f);
                            ImGui::TextColored(col, tg < 0.98f ? "HOT %3.0f%%" : "%3.0f%%",
                                               100.0f * std::min(heat, 9.99f));
                        }
                    }
                    ImGui::SameLine(432);
                    // channel finder: burst this raw output to locate its amp by feel
                    bool pinging = gOut.pingChan.load() == c;
                    char pid[16];
                    snprintf(pid, sizeof(pid), pinging ? "...##p%d" : "PING##p%d", c);
                    if (ImGui::SmallButton(pid) && gOut.running()) gOut.ping(c);
                }
                ImGui::TextDisabled(
                    "green = feeding its amp · amber = assigned, no signal · dim = unused · PING = burst that jack");
                {
                    float lim = gEngine.params.thermalLimit.load();
                    ImGui::TextDisabled("THERMAL GUARD   ·   45 s RMS cap per transducer channel · HOT = easing down");
                    ImGui::SetNextItemWidth(160);
                    if (ImGui::SliderFloat("##therm", &lim, 0.0f, 0.8f, lim > 0.001f ? "RMS cap %.2f" : "OFF"))
                        gEngine.params.thermalLimit.store(lim);
                }

                // ring isolation: solo one zone at the send, silence the rest
                ImGui::Spacing();
                ImGui::TextDisabled("ISOLATE RING   ·   solo one zone, all others silent   ·   keys 1-5 / 0 = off");
                ImGui::Separator();
                {
                    int solo = gEngine.params.soloZone.load();
                    for (int i = 0; i < NZONES; i++) {
                        int z = kTuneOrder[i]; // 1 = FEET … 5 = HEAD, matching the tuner
                        bool on = solo == z;
                        if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.55f, 0.25f, 0.85f));
                        char lbl[24];
                        snprintf(lbl, sizeof(lbl), "%d %s", i + 1, kZoneNames[z]);
                        if (ImGui::Button(lbl, ImVec2(92, 26))) setSoloZone(z);
                        if (on) ImGui::PopStyleColor();
                        ImGui::SameLine();
                    }
                    if (solo >= 0) {
                        if (ImGui::Button("ALL", ImVec2(60, 26))) setSoloZone(-1);
                        ImGui::SameLine();
                        if (ImGui::Button("PULSE", ImVec2(60, 26))) fireZonePulse(solo);
                        int c2 = gEngine.params.zoneChan2[solo].load();
                        if (c2 >= 0)
                            ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1),
                                               "SOLO %s  ->  Ch %d + %d only", kZoneNames[solo],
                                               gEngine.params.zoneChan[solo].load() + 1, c2 + 1);
                        else
                            ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1),
                                               "SOLO %s  ->  Ch %d only", kZoneNames[solo],
                                               gEngine.params.zoneChan[solo].load() + 1);
                    } else {
                        ImGui::TextDisabled("no solo — all rings live");
                    }
                }

                // per zone: is the engine even generating vibration?
                ImGui::Spacing();
                ImGui::TextDisabled("ZONE ENGINE   ·   vibration being generated");
                ImGui::Separator();
                for (int z = 0; z < NZONES; z++) {
                    float lvl = gEngine.meter[2 + z].load();
                    int zc = gEngine.params.zoneChan[z].load();
                    ImGui::Text("%-6s", kZoneNames[z]);
                    ImGui::SameLine(78);
                    ImGui::TextDisabled("%3.0f Hz", gEngine.zoneHz[z].load());
                    ImGui::SameLine(140);
                    ImGui::TextDisabled("trim x%.2f", gEngine.params.zoneTrim[z].load());
                    ImGui::SameLine(238);
                    ImVec2 mp = ImGui::GetCursorScreenPos();
                    float mw = 140, mh = 8;
                    hdl->AddRectFilled(ImVec2(mp.x, mp.y + 5), ImVec2(mp.x + mw, mp.y + 5 + mh),
                                       IM_COL32(255, 255, 255, 18), 4);
                    if (lvl > 0.01f)
                        hdl->AddRectFilled(ImVec2(mp.x, mp.y + 5),
                                           ImVec2(mp.x + mw * std::min(1.0f, lvl), mp.y + 5 + mh),
                                           IM_COL32(190, 160, 255, 230), 4);
                    ImGui::Dummy(ImVec2(mw + 8, 0));
                    ImGui::SameLine();
                    int zc2 = gEngine.params.zoneChan2[z].load();
                    if (zc < 0)
                        ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "OFF");
                    else if (zc2 >= 0)
                        ImGui::TextDisabled("Ch %d+%d", zc + 1, zc2 + 1);
                    else
                        ImGui::TextDisabled("Ch %d", zc + 1);
                }
            }
            ImGui::End();
        }

        // ── octagon (left) ──
        float rightW = std::min(700.0f, W_ * 0.46f);
        float leftW = W_ - rightW;
        ImVec2 octC(leftW * 0.5f, H_ * 0.47f);
        float octR = std::min(leftW, H_) * 0.42f;

        if (gMode == 2) drawOctagonLauncher(dl, octC, octR);
        else drawOctagon(dl, octC, octR);



        // ── transport pill under octagon ──
        {
            float pw = 470, ph = 56;
            ImVec2 p0(octC.x - pw / 2, H_ - ph - 26), p1(octC.x + pw / 2, H_ - 26);
            dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 20, 235), ph / 2);
            dl->AddRect(p0, p1, W(0.08f), ph / 2);

            // waveform glyph
            float gx = p0.x + 30, gy = p0.y + ph / 2;
            for (int i = 0; i < 5; i++) {
                float hh = (i == 2 ? 10.f : (i == 1 || i == 3) ? 7.f : 4.f);
                dl->AddRectFilled(ImVec2(gx + i * 5, gy - hh), ImVec2(gx + i * 5 + 2.5f, gy + hh), W(0.85f), 1);
            }
            // Output (device select)
            ImGui::SetCursorScreenPos(ImVec2(gx + 34, p0.y + 14));
            std::string devLabel = "Output";
            if (ImGui::Button(devLabel.c_str(), ImVec2(86, 28))) ImGui::OpenPopup("##devices");
            if (ImGui::BeginPopup("##devices")) {
                int nch = (gSelDevice >= 0 && gSelDevice < (int)gDevices.size())
                              ? gDevices[gSelDevice].channels : 2;
                auto gainSlider = [&](const char* id, std::atomic<float>& a,
                                      float maxV, const char* fmt) {
                    float v = a.load();
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::SliderFloat(id, &v, 0.0f, maxV, fmt)) a.store(v);
                };
                // stereo-pair stepper: Off, 1-2, 3-4, …
                auto pairRow = [&](const char* name, const char* tag,
                                   std::atomic<int>& a) {
                    int v = a.load();
                    ImGui::Text("%s", name); ImGui::SameLine(150);
                    char idm[24], idp[24];
                    snprintf(idm, sizeof(idm), "-##%s", tag);
                    snprintf(idp, sizeof(idp), "+##%s", tag);
                    if (ImGui::SmallButton(idm)) {
                        v = v <= 0 ? -1 : v - 2;
                        a.store(v);
                    }
                    ImGui::SameLine();
                    if (v < 0) ImGui::Text("Off");
                    else ImGui::Text("Ch %d-%d", v + 1, v + 2);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(idp)) {
                        v = v < 0 ? 0 : v + 2;
                        if (v > nch - 2) v = std::max(0, nch - 2);
                        a.store(v);
                    }
                };

                ImGui::TextDisabled("OUTPUT DEVICE");
                ImGui::Separator();
                for (int i = 0; i < (int)gDevices.size(); i++) {
                    char row[300];
                    snprintf(row, sizeof(row), "%s  ·  %dch  ·  %.0fk%s", gDevices[i].name.c_str(),
                             gDevices[i].channels, gDevices[i].sampleRate / 1000.0,
                             i == gSelDevice ? "   ●" : "");
                    if (ImGui::Selectable(row)) {
                        gSelDevice = i;
                        if (gPlaying) { stopAudio(); startAudio(); }
                    }
                }
                if (ImGui::Selectable("Rescan devices")) gDevices = listOutputDevices();

                // ── music + surround on the selected device ──
                ImGui::Separator();
                ImGui::TextDisabled("MUSIC   ·   front pair");
                pairRow("Music / Hi-Fi", "m", gEngine.params.musicChanL);
                ImGui::Text("Level"); ImGui::SameLine(150);
                gainSlider("##mg", gEngine.params.musicGain, 1.5f, "%.2f");
                // sync look-ahead: the speakers are held back this much so every
                // detected hit and note can be placed at its true onset
                ImGui::Text("Sync"); ImGui::SameLine(150);
                {
                    float la = gEngine.params.syncLookaheadMs.load();
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::SliderFloat("##la", &la, 0.0f, 200.0f, "%.0f ms look-ahead"))
                        gEngine.params.syncLookaheadMs.store(la);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Music is held back this much so felt and heard land together.\n0 = off (felt trails heard by 15-40 ms). 80 ms covers every detector.\nVideo lip-sync drifts above ~100 ms.");
                }

                ImGui::Separator();
                ImGui::TextDisabled("SURROUND   ·   rear / side pair");
                pairRow("Surround", "s", gEngine.params.surrChanL);
                ImGui::Text("Level"); ImGui::SameLine(150);
                gainSlider("##sg", gEngine.params.surrGain, 1.5f, "%.2f");
                ImGui::Text("Width"); ImGui::SameLine(150);
                gainSlider("##sw", gEngine.params.surrWidth, 1.0f, "%.2f");

                // ── vibration zones: channel + trim + test pulse ──
                ImGui::Separator();
                ImGui::TextDisabled("VIBRATION ZONES   (device has %d out)", nch);
                for (int z = 0; z < NZONES; z++) {
                    int c = gEngine.params.zoneChan[z].load();
                    ImGui::Text("%s", kZoneNames[z]); ImGui::SameLine(80);
                    char idm[16], idp[16], idt[16], idr[16];
                    snprintf(idm, sizeof(idm), "-##z%d", z);
                    snprintf(idp, sizeof(idp), "+##z%d", z);
                    snprintf(idt, sizeof(idt), "##tr%d", z);
                    snprintf(idr, sizeof(idr), "TEST##%d", z);
                    if (ImGui::SmallButton(idm)) {
                        c = c <= -1 ? -1 : c - 1;
                        gEngine.params.zoneChan[z].store(c);
                    }
                    ImGui::SameLine();
                    if (c < 0) ImGui::Text("Off"); else ImGui::Text("Ch %d", c + 1);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(idp)) {
                        c = std::min(c + 1, nch - 1);
                        gEngine.params.zoneChan[z].store(c);
                    }
                    // second send (same signal to another amp channel; in STEREO
                    // mode the right side of the ring), Off = none
                    {
                        int c2 = gEngine.params.zoneChan2[z].load();
                        char id2m[16], id2p[16];
                        snprintf(id2m, sizeof(id2m), "-##z2%d", z);
                        snprintf(id2p, sizeof(id2p), "+##z2%d", z);
                        ImGui::SameLine();
                        ImGui::TextDisabled(gEngine.params.engineMode.load() == 3 ? "R" : "+");
                        ImGui::SameLine();
                        if (ImGui::SmallButton(id2m))
                            gEngine.params.zoneChan2[z].store(c2 <= -1 ? -1 : c2 - 1);
                        ImGui::SameLine();
                        if (c2 < 0) ImGui::TextDisabled("Off"); else ImGui::Text("Ch %d", c2 + 1);
                        ImGui::SameLine();
                        if (ImGui::SmallButton(id2p))
                            gEngine.params.zoneChan2[z].store(std::min(c2 + 1, nch - 1));
                    }
                    ImGui::SameLine(300);
                    float tv = gEngine.params.zoneTrim[z].load();
                    ImGui::SetNextItemWidth(110);
                    if (ImGui::SliderFloat(idt, &tv, 0.0f, 1.5f, "%.2f"))
                        gEngine.params.zoneTrim[z].store(tv);
                    ImGui::SameLine();
                    // tuner: fire a pulse at the zone's live carrier frequency
                    if (ImGui::SmallButton(idr)) {
                        float hz = std::max(zoneSweetLo(z), std::min(zoneSweetHi(z), gEngine.zoneHz[z].load()));
                        armZoneVoice(z, PAD_PURE);
                        gEngine.params.padHz[z].store(hz);
                        gEngine.params.padVel[z].store(1.0f);
                        gEngine.params.padGate[z].store(1);
                        gTestPulse[z] = 0.7f;
                    }
                }
#ifdef __APPLE__
                // ── live input: the interface's own inputs into the engine ──
                ImGui::Separator();
                ImGui::TextDisabled("LIVE INPUT   ·   guitar / mic on the interface, felt like the music");
                {
                    bool on = gInOn;
                    if (ImGui::Checkbox("Input on", &on)) setInput(on);
                    if (gInOn && gIn.running()) {
                        int nin = std::max(1, gIn.deviceChannels);
                        int cl = gIn.chanL.load() + 1, cr = gIn.chanR.load() + 1;
                        ImGui::SameLine(0, 14);
                        ImGui::SetNextItemWidth(70);
                        if (ImGui::InputInt("L##inl", &cl, 1, 1)) gIn.chanL.store(std::min(std::max(cl, 1), nin) - 1);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(70);
                        if (ImGui::InputInt("R##inr", &cr, 1, 1)) gIn.chanR.store(std::min(std::max(cr, 1), nin) - 1);
                        ImGui::SameLine();
                        float g = gIn.gain.load();
                        ImGui::SetNextItemWidth(110);
                        if (ImGui::SliderFloat("##ingain", &g, 0.0f, 8.0f, "gain %.1fx")) gIn.gain.store(g);
                        ImGui::SameLine();
                        float pk = gIn.peak.load();
                        ImVec2 mp = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(mp.x, mp.y + 6), ImVec2(mp.x + 80, mp.y + 14), IM_COL32(255, 255, 255, 18), 3);
                        if (pk > 0.003f)
                            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(mp.x, mp.y + 6), ImVec2(mp.x + 80 * std::min(1.0f, pk), mp.y + 14),
                                                                      pk > 0.98f ? IM_COL32(255, 120, 90, 255) : IM_COL32(140, 235, 255, 230), 3);
                        ImGui::Dummy(ImVec2(84, 0));
                        ImGui::TextDisabled("inputs 1-%d on %s  ·  same L and R = mono", nin, gDevices[gSelDevice].name.c_str());
                        bool hear = gOut.inToSpeakers.load();
                        if (ImGui::Checkbox("Also play the input on the speaker outputs", &hear)) gOut.inToSpeakers.store(hear);
                        if (ImGui::IsItemHovered()) ImGui::SetTooltip("off = the input is only FELT (your amp or the other interface makes the sound)");
                    } else if (gInOn) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(1, 0.75f, 0.3f, 1), "%s", gIn.lastError.c_str());
                    }
                }
#endif
                {
                    bool mon = gEngine.params.monitorVibOnStereo.load() != 0;
                    if (ImGui::Checkbox("Monitor vibration on stereo devices", &mon))
                        gEngine.params.monitorVibOnStereo.store(mon ? 1 : 0);
                }
                ImGui::EndPopup();
            }

            // transport buttons
            float bx = p0.x + 210, by = p0.y + ph / 2;
            // play/pause circle
            ImGui::SetCursorScreenPos(ImVec2(bx - 16, by - 16));
            if (ImGui::InvisibleButton("##play", ImVec2(32, 32))) {
                if (gPlaying) stopAudio(); else startAudio();
            }
            bool hov = ImGui::IsItemHovered();
            dl->AddCircleFilled(ImVec2(bx, by), 16, W(hov ? 0.95f : 0.9f));
            if (gPlaying) {
                dl->AddRectFilled(ImVec2(bx - 5.5f, by - 6), ImVec2(bx - 1.5f, by + 6), IM_COL32(10,10,10,255), 1);
                dl->AddRectFilled(ImVec2(bx + 1.5f, by - 6), ImVec2(bx + 5.5f, by + 6), IM_COL32(10,10,10,255), 1);
            } else {
                dl->AddTriangleFilled(ImVec2(bx - 4, by - 7), ImVec2(bx - 4, by + 7), ImVec2(bx + 7, by), IM_COL32(10,10,10,255));
            }
            // stop square
            ImGui::SetCursorScreenPos(ImVec2(bx + 26, by - 10));
            if (ImGui::InvisibleButton("##stop", ImVec2(20, 20))) stopAudio();
            dl->AddRectFilled(ImVec2(bx + 29, by - 6), ImVec2(bx + 41, by + 6), W(ImGui::IsItemHovered() ? 0.9f : 0.6f), 2);

            // volume slider
            float sx0 = bx + 66, sx1 = p1.x - 64, sy = by;
            dl->AddRectFilled(ImVec2(sx0, sy - 2), ImVec2(sx1, sy + 2), W(0.14f), 2);
            dl->AddRectFilled(ImVec2(sx0, sy - 2), ImVec2(sx0 + (sx1 - sx0) * gMasterVol, sy + 2), W(0.9f), 2);
            dl->AddCircleFilled(ImVec2(sx0 + (sx1 - sx0) * gMasterVol, sy), 9, W(0.97f));
            ImGui::SetCursorScreenPos(ImVec2(sx0 - 8, sy - 12));
            ImGui::InvisibleButton("##vol", ImVec2(sx1 - sx0 + 16, 24));
            if (ImGui::IsItemActive()) {
                gMasterVol = std::max(0.0f, std::min(1.0f, (io.MousePos.x - sx0) / (sx1 - sx0)));
                gEngine.params.masterVolume.store(gMasterVol);
                outCC(0, 7, gMasterVol);
            }
            char volTxt[8]; snprintf(volTxt, sizeof(volTxt), "%d", (int)std::lround(gMasterVol * 100));
            dl->AddText(ImVec2(sx1 + 18, sy - 8), W(0.85f), volTxt);
        }

        // ── right glass card ──
        float cardX = leftW - 6, cardY = 18, cardW = rightW - 18, cardH = H_ - 36;
        dl->AddRectFilled(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH),
                          IM_COL32(16, 16, 18, 245), 22);
        dl->AddRect(ImVec2(cardX, cardY), ImVec2(cardX + cardW, cardY + cardH), W(0.07f), 22);

        // mode tabs: Audio · Visual · Artifact
        {
            static const char* modes[3] = {"Audio", "Visual", "Artifact"};
            float tx = cardX + 24, ty = cardY + 18, tw = cardW - 48, th = 34;
            float each = tw / 3.0f;
            for (int t = 0; t < 3; t++) {
                ImVec2 b0(tx + t * each, ty), b1(tx + (t + 1) * each, ty + th);
                ImGui::SetCursorScreenPos(b0);
                char id[16]; snprintf(id, sizeof(id), "##mode%d", t);
                if (ImGui::InvisibleButton(id, ImVec2(each, th))) gMode = t;
                bool active = (t == gMode);
                if (active) {
                    dl->AddRectFilled(ImVec2(b0.x + 4, b0.y), ImVec2(b1.x - 4, b1.y), W(0.10f), th / 2);
                    dl->AddRect(ImVec2(b0.x + 4, b0.y), ImVec2(b1.x - 4, b1.y), W(0.12f), th / 2);
                }
                ImVec2 ts = ImGui::CalcTextSize(modes[t]);
                dl->AddText(ImVec2(b0.x + (each - ts.x) / 2, b0.y + (th - ts.y) / 2),
                            W(active ? 0.95f : (ImGui::IsItemHovered() ? 0.7f : 0.4f)), modes[t]);
            }
            // hairline under the mode row
            dl->AddLine(ImVec2(cardX + 24, ty + th + 12), ImVec2(cardX + cardW - 24, ty + th + 12), W(0.06f), 1.0f);
        }
        const float bodyY = cardY + 18 + 34 + 12 + 24; // mode row + hairline + one gutter

        if (gMode != 0) {
            // VISUAL / ARTIFACT: an ImGui child fills the card below the mode row
            ImGui::SetCursorScreenPos(ImVec2(cardX + 24, bodyY));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::BeginChild("##modebody", ImVec2(cardW - 48, cardH - (bodyY - cardY) - 20), false,
                              ImGuiWindowFlags_NoScrollbar);
            float bw = ImGui::GetContentRegionAvail().x;
            if (gMode == 1) {
                drawVisualBody(win);
            } else {
                // ARTIFACT sub-tabs: Sounds · Tuner · MIDI
                static const char* subs[3] = {"Sounds", "Tuner", "MIDI"};
                ImDrawList* cdl = ImGui::GetWindowDrawList();
                ImVec2 s0 = ImGui::GetCursorScreenPos();
                // secondary selector: plain words with an underline, no pills (the
                // pills belong to the mode row alone)
                float x = s0.x, sh = 22;
                for (int t = 0; t < 3; t++) {
                    ImVec2 ts = ImGui::CalcTextSize(subs[t]);
                    ImVec2 b0(x, s0.y), b1(x + ts.x, s0.y + sh);
                    ImGui::SetCursorScreenPos(b0);
                    char id[16]; snprintf(id, sizeof(id), "##sub%d", t);
                    if (ImGui::InvisibleButton(id, ImVec2(ts.x, sh))) gArtifactTab = t;
                    bool active = t == gArtifactTab;
                    cdl->AddText(b0, W(active ? 0.95f : (ImGui::IsItemHovered() ? 0.7f : 0.38f)), subs[t]);
                    if (active) cdl->AddLine(ImVec2(b0.x, b1.y + 2), ImVec2(b1.x, b1.y + 2), W(0.9f), 1.5f);
                    x += ts.x + 26;
                }
                ImGui::SetCursorScreenPos(ImVec2(s0.x, s0.y + sh + 18));
                if (gArtifactTab == 0) drawSoundsBody(bw);
                else if (gArtifactTab == 1) drawTunerBody();
                else drawMidiBody();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
        }

        // AUDIO: presets, master strip, zone columns, engine parameters
        if (gMode == 0) {
        // presets: four equal cells across the card, word centred, underline
        // under the word — the same rhythm as the mode row above it
        const float pad = 24;                 // card inset, one value everywhere
        const float gutter = 24;              // vertical rhythm between blocks
        {
            float tx = cardX + pad, ty = bodyY, tw = cardW - 2 * pad, th = 22;
            float each = tw / 5.0f;
            for (int t = 0; t < 5; t++) {
                ImVec2 ts = ImGui::CalcTextSize(kTabNames[t]);
                ImVec2 c0(tx + t * each, ty);
                ImGui::SetCursorScreenPos(c0);
                char id[16]; snprintf(id, sizeof(id), "##tab%d", t);
                if (ImGui::InvisibleButton(id, ImVec2(each, th))) {
                    gActiveTab = t;
                    applyPreset(kTabPreset[t]);
                }
                bool active = (t == gActiveTab);
                float wx = c0.x + (each - ts.x) / 2;
                dl->AddText(ImVec2(wx, ty), W(active ? 0.95f : (ImGui::IsItemHovered() ? 0.7f : 0.38f)), kTabNames[t]);
                if (active) dl->AddLine(ImVec2(wx, ty + th + 2), ImVec2(wx + ts.x, ty + th + 2), W(0.9f), 1.5f);
            }
        }

        // master strip
        {
            float mx = cardX + pad, my = bodyY + 22 + gutter, mw = cardW - 2 * pad, mh = 150;
            dl->AddRectFilled(ImVec2(mx, my), ImVec2(mx + mw, my + mh), IM_COL32(8, 8, 9, 255), 14);
            dl->AddRect(ImVec2(mx, my), ImVec2(mx + mw, my + mh), W(0.06f), 14);

            dl->AddText(ImVec2(mx + 16, my + 12), W(0.9f), "MASTER");
            float bpm = gEngine.analyzer.out.bpm.load();
            int key = gEngine.analyzer.out.keyIndex.load();
            int minor = gEngine.analyzer.out.keyMinor.load();
            char info[96];
            snprintf(info, sizeof(info), "%.0f BPM  ·  %s%s  ·  %.0f Hz",
                     bpm, kKeyNames[key], minor ? "m" : "",
                     gEngine.analyzer.out.f0Hz.load());
            dl->AddText(ImVec2(mx + 84, my + 12), W(0.32f), info);
            // engine mode: BODY (one root, five mixes) vs SPLIT (five layers of the song)
            {
                static const char* modes[5] = {"SYNTH", "SPLIT", "MONO", "STEREO", "BODY"};
                static const char* modeTips[5] = {
                    "SYNTH: one root note, five mixes of it. The original engine.",
                    "SPLIT: feet = sub, root = bass line, belly = drums,\nheart = vocal/chord melody, head = lead/air melody. Five layers at once.",
                    "MONO: the song itself, felt. Its low end to every zone,\nbass exaggerated x2, plus an octave-down copy of bass the rings can't move.",
                    "STEREO: MONO split left / right. Send 1 = left side, send 2 = right side\nof each ring (low-end width x2); the feet stay mono.",
                    "BODY: 3 channels. Head+heart = the voice, belly+root = bass line + drums,\nfeet = LFE sub (+10 dB). A head-to-toe roll fires only on a clear bass drop."};
                int em = gEngine.params.engineMode.load();
                float x = mx + mw - 16;
                for (int m = 4; m >= 0; m--) {
                    ImVec2 ts = ImGui::CalcTextSize(modes[m]);
                    x -= ts.x;
                    ImGui::SetCursorScreenPos(ImVec2(x - 6, my + 8));
                    char id[16]; snprintf(id, sizeof(id), "##em%d", m);
                    if (ImGui::InvisibleButton(id, ImVec2(ts.x + 12, 22))) gEngine.params.engineMode.store(m);
                    bool on = em == m;
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", modeTips[m]);
                    dl->AddText(ImVec2(x, my + 12), W(on ? 0.92f : (ImGui::IsItemHovered() ? 0.6f : 0.3f)), modes[m]);
                    if (on) dl->AddLine(ImVec2(x, my + 30), ImVec2(x + ts.x, my + 30), W(0.9f), 1.5f);
                    x -= 14;
                }
            }


            // five zone frequency lines — one string per zone, endpoints
            // stacked at each side (HEAD top → FEET bottom); each line's wave
            // count follows the zone's live carrier Hz, amplitude and glow
            // follow its meter, so playing a pad makes its line surge
            {
                float wy = my + 40, wh = mh - 48, wx = mx + 58, ww = mw - 72;
                static float phase[NZONES] = {0};
                static float ampSm[NZONES] = {0};
                float dt = ImGui::GetIO().DeltaTime;
                const int P = 96;
                for (int z = 0; z < NZONES; z++) {
                    float hz = gEngine.zoneHz[z].load();
                    float lvl = gEngine.meter[2 + z].load();
                    phase[z] += dt * hz * 0.35f; // slowed visual travel
                    // five separate lanes, HEAD at the top, so each layer's own
                    // motion reads on its own line
                    float lane = wh / (float)NZONES;
                    float ay = wy + lane * (z + 0.5f);
                    float cycles = std::max(2.0f, std::min(9.0f, hz / 11.0f));
                    float amp = 1.5f + (lane * 0.48f - 1.5f) * lvl;
                    ampSm[z] += (amp - ampSm[z]) * 0.12f; // eased motion
                    ImVec2 pts[P];
                    for (int i = 0; i < P; i++) {
                        float t = (float)i / (P - 1);
                        float taper = std::sin(dsp::kPi * t); // pins endpoints
                        float w = std::sin(2 * dsp::kPi * (cycles * t - phase[z]));
                        pts[i] = ImVec2(wx + ww * t, ay + taper * ampSm[z] * w);
                    }
                    dl->AddPolyline(pts, P, W(0.18f + 0.5f * lvl), 0,
                                    1.0f + 1.5f * lvl);
                    dl->AddCircleFilled(ImVec2(wx, ay), 2.0f, W(0.35f + 0.4f * lvl));
                    dl->AddCircleFilled(ImVec2(wx + ww, ay), 2.0f, W(0.35f + 0.4f * lvl));
                    if (gFontSmall) ImGui::PushFont(gFontSmall);
                    ImVec2 ls = ImGui::CalcTextSize(kZoneNames[z]);
                    dl->AddText(ImVec2(wx - ls.x - 8, ay - ls.y / 2), W(0.22f + 0.4f * lvl), kZoneNames[z]);
                    if (gFontSmall) ImGui::PopFont();
                }
            }
        }

        // zone columns: equal cells across the same inset as everything else
        const float paramsH = 3 * 48;         // three rows of label + bar
        const float paramsY = cardY + cardH - pad - paramsH;
        {
            float zx = cardX + pad, zy = bodyY + 22 + gutter + 150 + gutter, zw = cardW - 2 * pad;
            float zh = paramsY - gutter - zy;
            float each = zw / (float)NZONES;
            for (int z = 0; z < NZONES; z++)
                zoneColumn(z, zx + z * each, zy, each, zh);
        }

        // engine parameters: a 4 × 3 grid on the card inset. Every cell is
        // label over bar; State (a picker) and Void sit in the grid like the rest.
        {
            const float colGap = 24;
            float px = cardX + pad, pw = cardW - 2 * pad;
            float colW = (pw - 3 * colGap) / 4.0f;
            float rowH = 48;
            auto cx = [&](int c) { return px + c * (colW + colGap); };
            auto ry = [&](int r) { return paramsY + r * rowH; };
            miniParam("Intensity", gEngine.params.intensity, cx(0), ry(0), colW);
            miniParam("Balance", gBalance, cx(1), ry(0), colW);
            miniParam("Pulse", gEngine.params.pulseSync, cx(2), ry(0), colW);
            miniParam("Flow", gEngine.params.bodyFlow, cx(3), ry(0), colW);
            miniParam("Warmth", gEngine.params.waveWarmth, cx(0), ry(1), colW);
            miniParam("Depth", gEngine.params.subDepth, cx(1), ry(1), colW);
            miniParam("Breath", gEngine.params.breath, cx(2), ry(1), colW);
            miniParam("Dynamics", gEngine.params.dynamics, cx(3), ry(1), colW);
            miniParam("Void", gEngine.params.voidAmt, cx(1), ry(2), colW);
            miniParam("Lift", gEngine.params.lift, cx(2), ry(2), colW);
            // Balance drives the grounded↔uplifted pair
            float bal = gBalance.load();
            gEngine.params.uplift.store(bal);
            gEngine.params.grounding.store(1.0f - bal);
            // void flash: the live duck shown along the bar
            {
                float vn = gEngine.voidNow.load();
                if (vn > 0.02f)
                    dl->AddRectFilled(ImVec2(cx(1), ry(2) + 18), ImVec2(cx(1) + colW * vn, ry(2) + 21),
                                      IM_COL32(255, 255, 255, (int)(150 * vn)), 2);
            }
            // State: label like the others, the picker where a bar would be
            {
                static const char* bw[5] = {"Sleep", "Dream", "Calm", "Focus", "Peak"};
                int cur = gEngine.params.brainwave.load();
                dl->AddText(ImVec2(cx(0), ry(2)), W(0.35f), "State");
                ImGui::SetCursorScreenPos(ImVec2(cx(0), ry(2) + 20));
                ImGui::SetNextItemWidth(colW);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 2));
                if (ImGui::BeginCombo("##state", bw[cur])) {
                    for (int b = 0; b < 5; b++) {
                        char row[32];
                        snprintf(row, sizeof(row), "%s   ·  %.0f Hz", bw[b], kBrainwaveHz[b]);
                        if (ImGui::Selectable(row, b == cur)) gEngine.params.brainwave.store(b);
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleVar();
            }
            // tap/output error, if any, in the free cells of the last row
            if (!gTap.running() && !gTap.lastError.empty())
                dl->AddText(ImVec2(cx(3), ry(2) + 4), IM_COL32(255, 120, 120, 200), gTap.lastError.c_str());
            else if (!gOut.running() && !gOut.lastError.empty())
                dl->AddText(ImVec2(cx(3), ry(2) + 4), IM_COL32(255, 120, 120, 200), gOut.lastError.c_str());
        }
        } // gMode == 0

        // ── status lights: audio signal dot, MIDI in/out (top of the canvas).
        //    The whole cluster is a button: click it for the SYSTEM HEALTH window ──
        {
            {
                ImGui::SetCursorScreenPos(ImVec2(leftW - 330, 28));
                if (ImGui::InvisibleButton("##healthbtn", ImVec2(300, 28))) gShowHealth = !gShowHealth;
                bool hh = ImGui::IsItemHovered();
                if (hh) {
                    dl->AddRectFilled(ImVec2(leftW - 330, 28), ImVec2(leftW - 30, 56), W(0.05f), 14);
                    ImGui::SetTooltip("System health: interface, channels, isolate rings, heat");
                }
                if (gShowHealth || hh) {
                    if (gFontSmall) ImGui::PushFont(gFontSmall);
                    ImVec2 ts = ImGui::CalcTextSize("HEALTH");
                    dl->AddText(ImVec2(leftW - 40 - ts.x, 42 - ts.y / 2), W(gShowHealth ? 0.9f : 0.5f), "HEALTH");
                    if (gFontSmall) ImGui::PopFont();
                }
            }
            {
                bool signal = gTap.running() && gTap.inputPeak.load() > 0.003f;
                ImVec2 dp(leftW - 100, 42);
                if (signal) dl->AddCircleFilled(dp, 9, IM_COL32(80, 220, 120, 45));
                dl->AddCircleFilled(dp, 4.5f,
                                    signal ? IM_COL32(80, 220, 120, 255) : W(0.20f));
            }
#ifdef __APPLE__
            // MIDI trust light: square blinks on every incoming event, device
            // name shows while a controller is connected — answers "is it
            // plugged in?" at a glance
            {
                double now = glfwGetTime();
                uint32_t act = gMidi.activity.load();
                if (act != gMidiActPrev) { gMidiActPrev = act; gMidiActFlashT = now; }
                bool conn = gMidi.connected();
                bool flash = now - gMidiActFlashT < 0.12;
                ImVec2 mp(leftW - 134, 42);
                ImU32 col = flash ? IM_COL32(255, 255, 255, 255)
                          : conn ? IM_COL32(80, 220, 120, 200) : W(0.15f);
                dl->AddRectFilled(ImVec2(mp.x - 4, mp.y - 4),
                                  ImVec2(mp.x + 4, mp.y + 4), col, 1.5f);
                // MIDI OUT light: upward triangle, blinks on every message sent
                uint32_t oact = gMidiOut.activity.load();
                if (oact != gOutActPrev) { gOutActPrev = oact; gOutFlashT = now; }
                bool oflash = now - gOutFlashT < 0.12;
                bool oon = gOutOn && gMidiOut.running();
                ImVec2 op(leftW - 168, 42);
                ImU32 ocol = oflash ? IM_COL32(255, 255, 255, 255)
                           : oon ? IM_COL32(80, 220, 120, 200) : W(0.15f);
                dl->AddTriangleFilled(ImVec2(op.x, op.y - 5), ImVec2(op.x + 5, op.y + 4),
                                      ImVec2(op.x - 5, op.y + 4), ocol);
                if (conn) {
                    std::string nm = gMidi.deviceName();
                    if (nm.size() > 22) nm = nm.substr(0, 22);
                    for (auto& c : nm) c = (char)toupper((unsigned char)c);
                    if (gFontSmall) ImGui::PushFont(gFontSmall);
                    ImVec2 ts = ImGui::CalcTextSize(nm.c_str());
                    dl->AddText(ImVec2(op.x - 14 - ts.x, op.y - ts.y / 2),
                                W(flash ? 0.9f : 0.45f), nm.c_str());
                    if (gFontSmall) ImGui::PopFont();
                }
            }
#endif
        }

        ImGui::End();

        // ── render shader passes (before ImGui draw) ──
        if (gShaders.active()) {
            AudioUniforms au;
            au.level = gEngine.audioLevel.load();
            au.bass = gEngine.audioBass.load();
            au.mid = gEngine.audioMid.load();
            au.high = gEngine.audioHigh.load();
            au.sub = gEngine.audioSubBand.load();
            au.lowMid = gEngine.audioLowMidBand.load();
            au.onset = gEngine.analyzer.out.onsetFlash.load();
            au.bpm = gEngine.analyzer.out.bpm.load();
            au.spectrum = gEngine.analyzer.uiSpectrum;
            ImVec2 mp = ImGui::GetIO().MousePos;
            float mx = vp->Size.x > 0 ? mp.x / vp->Size.x : 0.5f;
            float my = vp->Size.y > 0 ? 1.0f - mp.y / vp->Size.y : 0.5f;
            gShaders.render((float)(glfwGetTime() - gShaderT0), au, mx, my,
                            ImGui::IsMouseDown(0) ? 1.0f : 0.0f);
        }

        ImGui::Render();
        int dw, dh;
        glfwGetFramebufferSize(win, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(0.016f, 0.016f, 0.018f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (gShaders.active())
            gShaders.drawFullscreen(gShaders.outputTexture(), 1.0f, gEdgeFade);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);

        // ── external display/projector output ──
        if (gExtWin) {
            if (glfwWindowShouldClose(gExtWin)) {
                glfwDestroyWindow(gExtWin);
                gExtWin = nullptr;
                glfwMakeContextCurrent(win);
            } else {
                glfwMakeContextCurrent(gExtWin);
                int ew, eh;
                glfwGetFramebufferSize(gExtWin, &ew, &eh);
                glViewport(0, 0, ew, eh);
                glClearColor(0, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);
                if (gShaders.active())
                    gShaders.drawFullscreen(gShaders.outputTexture(), 1.0f, gEdgeFade);
                glfwSwapBuffers(gExtWin);
                glfwMakeContextCurrent(win);
            }
        }
    }

    stopAudio();
    gTap.stop();
#ifdef __APPLE__
    outNoteOff();
    gMidiOut.stop();
    gMidi.stop();
#endif
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
#else
int main(int argc, char** argv) { (void)argc; (void)argv; return runSelfTest(); }
#endif

// ─────────────────────────────────────────────────────────────────────────
// Self-test implementation
// ─────────────────────────────────────────────────────────────────────────
#include <cstdlib>

static bool writeToneWav(const char* path, float freq, float secs, int sr = 48000) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    int n = (int)(secs * sr);
    int dataBytes = n * 2 * 2;
    unsigned char hdr[44] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',
                             16,0,0,0,1,0,2,0,0,0,0,0,0,0,0,0,4,0,16,0,'d','a','t','a',0,0,0,0};
    auto put32 = [&](int off, unsigned v) { for (int i = 0; i < 4; i++) hdr[off + i] = (v >> (8 * i)) & 0xff; };
    put32(4, 36 + dataBytes); put32(24, sr); put32(28, sr * 4); put32(40, dataBytes);
    fwrite(hdr, 1, 44, f);
    for (int i = 0; i < n; i++) {
        float env = std::min(1.0f, std::min(i / 4800.0f, (n - i) / 4800.0f));
        short s = (short)(0.5f * env * 32767.0f * std::sin(2.0 * M_PI * freq * i / sr));
        for (int c = 0; c < 2; c++) fwrite(&s, 2, 1, f);
    }
    fclose(f);
    return true;
}

static int runSelfTest() {
    printf("── AVA OS self-test ──\n");
    int failures = 0;

    // 1) engine DSP on synthetic material: 55 Hz pad + 120 BPM kick
    {
        Engine eng;
        eng.init();
        eng.params.syncLookaheadMs.store(0); // windows below are relative to the input
        const int block = 512, sr = 48000;
        std::vector<float> L(block), R(block);
        std::vector<float> vibBuf[NZONES];
        float* vib[NZONES];
        for (int z = 0; z < NZONES; z++) { vibBuf[z].assign(block, 0); vib[z] = vibBuf[z].data(); }
        double zoneAcc[NZONES] = {0};
        long total = 0;
        float kickPhase = 0;
        for (int b = 0; b < (int)(20.0f * sr / block); b++) {
            for (int i = 0; i < block; i++) {
                long t = (long)b * block + i;
                float pad = 0.25f * std::sin(2.0 * M_PI * 55.0 * t / sr)
                          + 0.1f * std::sin(2.0 * M_PI * 220.0 * t / sr);
                // kick every 0.5 s (120 BPM)
                float tb = std::fmod(t / (float)sr, 0.5f);
                float kick = 0;
                if (tb < 0.12f) {
                    kickPhase += 2.0f * M_PI * (90.0f - 500.0f * tb) / sr;
                    kick = 0.8f * std::exp(-tb * 30.0f) * std::sin(kickPhase);
                }
                L[i] = pad + kick;
                R[i] = pad * 0.9f + kick;
            }
            eng.process(L.data(), R.data(), vib, block);
            if (b > (int)(4.0f * sr / block)) { // skip warmup
                for (int z = 0; z < NZONES; z++)
                    for (int i = 0; i < block; i++) zoneAcc[z] += (double)vib[z][i] * vib[z][i];
                total += block;
            }
        }
        // capture tempo/key from the rhythmic section before the beatless tail
        float bpm = eng.analyzer.out.bpm.load();
        float root = eng.analyzer.out.f0Hz.load();   // lowest pitch: the 55 Hz pad
        float rootX2 = eng.analyzer.out.rootHz.load();

        // breath/dynamics: quiet ambient tail (-18 dB pad, no beat) must pull
        // vibrations down much harder than the input level drop alone
        double loudVib = 0, loudN = 0;
        for (int z = 0; z < NZONES; z++) loudVib += zoneAcc[z];
        loudN = std::max(1L, total);
        double quietAcc = 0;
        long quietN = 0;
        for (int b = 0; b < (int)(8.0f * sr / block); b++) {
            for (int i = 0; i < block; i++) {
                long t = (long)b * block + i;
                float pad = 0.03f * std::sin(2.0 * M_PI * 55.0 * t / sr); // ~-18 dB, beatless
                L[i] = pad; R[i] = pad;
            }
            eng.process(L.data(), R.data(), vib, block);
            if (b > (int)(3.0f * sr / block)) {
                for (int z = 0; z < NZONES; z++)
                    for (int i = 0; i < block; i++) quietAcc += (double)vib[z][i] * vib[z][i];
                quietN += block;
            }
        }
        float loudRms = std::sqrt(loudVib / loudN);
        float quietRms = std::sqrt(quietAcc / std::max(1L, quietN));
        float ratio = loudRms > 0 ? quietRms / loudRms : 1.0f;
        printf("breath: vib RMS loud=%.4f quiet=%.4f ratio=%.3f (input ratio 0.12) %s\n",
               loudRms, quietRms, ratio, ratio < 0.10f ? "PASS" : "FAIL");
        if (ratio >= 0.10f) failures++;

        // fade test: 8 s linear fade-out of the pad → vibration RMS per second
        // must dim smoothly (monotonic, no cliff, ends near zero)
        {
            float fadeRms[8] = {0};
            long secN = 0;
            int blocksPerSec = (int)((float)sr / block);
            for (int sec = 0; sec < 8; sec++) {
                double acc = 0;
                secN = 0;
                for (int b = 0; b < blocksPerSec; b++) {
                    for (int i = 0; i < block; i++) {
                        long t = (long)(sec * blocksPerSec + b) * block + i;
                        float fade = 1.0f - (float)(sec * blocksPerSec * block + b * block + i)
                                             / (8.0f * blocksPerSec * block);
                        float pad = 0.25f * fade * std::sin(2.0 * M_PI * 55.0 * t / sr);
                        L[i] = pad; R[i] = pad;
                    }
                    eng.process(L.data(), R.data(), vib, block);
                    for (int z = 0; z < NZONES; z++)
                        for (int i = 0; i < block; i++) acc += (double)vib[z][i] * vib[z][i];
                    secN += block;
                }
                fadeRms[sec] = std::sqrt(acc / std::max(1L, secN));
            }
            printf("fade: vib RMS/s ");
            for (int s = 0; s < 8; s++) printf("%.3f ", fadeRms[s]);
            bool mono = true;
            for (int s = 2; s < 8; s++) if (fadeRms[s] > fadeRms[s - 1] * 1.15f) mono = false;
            bool ends = fadeRms[7] < 0.10f * std::max(fadeRms[0], fadeRms[1]);
            printf("→ %s\n", (mono && ends) ? "PASS smooth dim" : "FAIL");
            if (!(mono && ends)) failures++;
        }

        // void: on hot material with hard kicks and voidAmt=1, vibration just
        // after each kick must collapse vs the open window between kicks
        {
            Engine ev;
            ev.init();
            ev.params.voidAmt.store(1.0f);
            ev.params.syncLookaheadMs.store(0);
            float kp = 0;
            double duckAcc = 0, openAcc = 0;
            long duckN = 0, openN = 0;
            for (int b = 0; b < (int)(24.0f * sr / block); b++) {
                for (int i = 0; i < block; i++) {
                    long t = (long)b * block + i;
                    float pad = 0.35f * std::sin(2.0 * M_PI * 55.0 * t / sr)
                              + 0.1f * std::sin(2.0 * M_PI * 220.0 * t / sr);
                    float tb = std::fmod(t / (float)sr, 0.5f);
                    float kick = 0;
                    if (tb < 0.12f) {
                        kp += 2.0f * M_PI * (90.0f - 500.0f * tb) / sr;
                        kick = 0.9f * std::exp(-tb * 30.0f) * std::sin(kp);
                    }
                    L[i] = pad + kick;
                    R[i] = pad + kick;
                }
                ev.process(L.data(), R.data(), vib, block);
                if (b > (int)(8.0f * sr / block)) { // let hot gate + BPM settle
                    for (int i = 0; i < block; i++) {
                        long t = (long)b * block + i;
                        float tb = std::fmod(t / (float)sr, 0.5f);
                        double e = 0;
                        for (int z = 0; z < NZONES; z++) e += (double)vib[z][i] * vib[z][i];
                        if (tb > 0.05f && tb < 0.15f) { duckAcc += e; duckN++; }
                        else if (tb > 0.35f && tb < 0.48f) { openAcc += e; openN++; }
                    }
                }
            }
            float duckRms = std::sqrt(duckAcc / std::max(1L, duckN));
            float openRms = std::sqrt(openAcc / std::max(1L, openN));
            float vr = openRms > 1e-6f ? duckRms / openRms : 1.0f;
            printf("void: RMS post-kick=%.4f open=%.4f ratio=%.3f %s\n",
                   duckRms, openRms, vr, (vr < 0.40f && openRms > 0.02f) ? "PASS" : "FAIL");
            if (!(vr < 0.40f && openRms > 0.02f)) failures++;
        }

        printf("engine: BPM=%.1f (expect ~120)  lowest pitch=%.1f Hz (expect 55)  x2=%.1f Hz\n", bpm, root, rootX2);
        bool bpmOK = bpm > 100 && bpm < 140;
        bool rootOK = root >= 53 && root <= 57 && std::fabs(rootX2 - 2 * root) < 0.5f;
        if (!bpmOK) { printf("  FAIL bpm\n"); failures++; } else printf("  PASS bpm\n");
        if (!rootOK) { printf("  FAIL root\n"); failures++; } else printf("  PASS root\n");
        static const char* zn[5] = {"HEAD", "HEART", "BELLY", "ROOT", "FEET"};
        for (int z = 0; z < NZONES; z++) {
            float rms = std::sqrt(zoneAcc[z] / std::max(1L, total));
            printf("  zone %-5s RMS=%.4f %s\n", zn[z], rms, rms > 0.02f ? "PASS" : "FAIL");
            if (rms <= 0.02f) failures++;
        }
    }

    // 2) output devices
    {
        auto devs = listOutputDevices();
        printf("output devices (%zu):\n", devs.size());
        for (auto& d : devs)
            printf("  [%u] %-40s %dch @ %.0f Hz\n", d.id, d.name.c_str(), d.channels, d.sampleRate);
        if (devs.empty()) { printf("  FAIL no output devices\n"); failures++; }
    }

    // 3) live system-audio tap: play a 440 Hz tone via afplay, verify capture
    {
        StereoRing ring;
        ring.init(48000 * 4);
        SystemTap tap;
        if (!tap.start(&ring)) {
            printf("tap: FAIL start — %s\n", tap.lastError.c_str());
            failures++;
        } else {
            printf("tap: started (agg rate %.0f Hz), playing test tone...\n", tap.tapSampleRate);
            { std::string tone = selftestTonePath();
              writeToneWav(tone.c_str(), 440.0f, 3.0f);
              playToneAsync(tone); }
            std::this_thread::sleep_for(std::chrono::milliseconds(3200));

            int avail = ring.available();
            float peak = tap.inputPeak.load();
            printf("tap: captured frames=%d  peak=%.4f\n", avail, peak);

            // dominant-frequency check via Goertzel at 440 vs 100/1000 Hz controls
            std::vector<float> L(avail), R(avail);
            ring.pop(L.data(), R.data(), avail);
            auto goertzel = [&](float f) {
                double sr = tap.tapSampleRate;
                double w = 2.0 * M_PI * f / sr, c = 2.0 * std::cos(w);
                double s0 = 0, s1 = 0, s2 = 0;
                for (int i = 0; i < avail; i++) { s0 = L[i] + c * s1 - s2; s2 = s1; s1 = s0; }
                return s1 * s1 + s2 * s2 - c * s1 * s2;
            };
            double p440 = goertzel(440), p100 = goertzel(100), p1000 = goertzel(1000);
            printf("tap: goertzel 440Hz=%.3g  100Hz=%.3g  1000Hz=%.3g\n", p440, p100, p1000);
            bool sigOK = avail > 48000 && peak > 0.01f && p440 > 10 * p100 && p440 > 10 * p1000;
            printf("  %s system-audio signal\n", sigOK ? "PASS" : "FAIL");
            if (!sigOK) failures++;
            tap.stop();
        }
    }

    printf("── self-test %s (%d failure%s) ──\n", failures ? "FAILED" : "PASSED",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}

// Compile the whole ShaderClaw3 library headlessly; render the default and
// verify non-black pixels with synthetic audio uniforms.
#ifndef TEMPLE_SELFTEST_ONLY
static int runShaderTest() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* w = glfwCreateWindow(64, 64, "st", nullptr, nullptr);
    glfwMakeContextCurrent(w);

    ShaderHost host;
    host.renderW = 320; host.renderH = 180;
    if (!host.loadLibrary(shaderLibraryDir())) {
        printf("FAIL: %s\n", host.lastError.c_str());
        return 1;
    }
    printf("── shader test: %zu library entries ──\n", host.entries().size());
    int ok = 0;
    std::vector<std::string> fails;
    for (int i = 0; i < (int)host.entries().size(); i++) {
        if (host.load(i)) ok++;
        else fails.push_back(host.entries()[i].title + " — " +
                             host.lastError.substr(0, 90));
    }
    printf("compile: %d/%zu OK\n", ok, host.entries().size());
    for (size_t i = 0; i < fails.size() && i < 12; i++) printf("  FAIL %s\n", fails[i].c_str());
    if (fails.size() > 12) printf("  … and %zu more\n", fails.size() - 12);

    // render + AUDIO REACTIVITY check: render the same shader twice with
    // identical TIME but silent vs loud audio — the pixel difference is the
    // proof that visuals move with the music.
    auto capture = [&](int idx, bool loud, std::vector<unsigned char>& px) -> bool {
        if (!host.load(idx)) return false; // fresh state both runs
        float spec[256];
        for (int i = 0; i < 256; i++)
            spec[i] = loud ? 0.4f + 0.3f * std::sin(i * 0.1f) : 0.0f;
        AudioUniforms au;
        if (loud) {
            au.level = 0.7f; au.bass = 0.8f; au.mid = 0.5f; au.high = 0.4f;
            au.sub = 0.7f; au.lowMid = 0.5f; au.onset = 0.9f; au.bpm = 124;
        }
        au.spectrum = spec;
        for (int f = 0; f < 40; f++) host.render(f / 30.0f, au, 0.5f, 0.5f, 0);
        px.assign(320 * 180 * 4, 0);
        glBindTexture(GL_TEXTURE_2D, host.outputTexture());
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        return true;
    };
    // full sweep: render + reactivity for EVERY shader; emit a skip list for
    // anything that renders black so broken shaders never ship
    int reactive = 0, staticN = 0, blackN = 0, probed = 0;
    std::vector<std::string> blackList, staticList;
    FILE* skipF = fopen("/tmp/ava_skip.txt", "w");
    fprintf(skipF, "# shaders excluded by full sweep (rendered black)\n");
    for (int li = 0; li < (int)host.entries().size(); li++) {
        const std::string& pf = host.entries()[li].file;
        std::vector<unsigned char> a, b;
        if (!capture(li, false, a) || !capture(li, true, b)) {
            blackN++;
            blackList.push_back(pf + " (compile)");
            fprintf(skipF, "%s\n", pf.c_str());
            continue;
        }
        probed++;
        long sumB = 0; double diff = 0;
        for (size_t i = 0; i < b.size(); i += 4) {
            sumB += b[i] + b[i + 1] + b[i + 2];
            diff += std::abs((int)b[i] - (int)a[i]) + std::abs((int)b[i + 1] - (int)a[i + 1]);
        }
        double diffAvg = diff / (b.size() / 4);
        if (sumB <= 100000) {
            blackN++;
            blackList.push_back(pf);
            fprintf(skipF, "%s\n", pf.c_str());
        } else if (diffAvg > 1.0) {
            reactive++;
        } else {
            staticN++;
            staticList.push_back(pf);
        }
        if ((li % 20) == 0) { printf("  … %d/%zu\n", li, host.entries().size()); fflush(stdout); }
    }
    fclose(skipF);
    printf("sweep: %d probed → %d REACTIVE · %d static · %d black/broken\n",
           probed, reactive, staticN, blackN);
    for (auto& s : blackList) printf("  BLACK  %s\n", s.c_str());
    for (auto& s : staticList) printf("  STATIC %s\n", s.c_str());
    printf("skip list written: /tmp/ava_skip.txt (%d entries)\n", blackN);
    bool renderOK = probed > 0;
    bool pass = ok > (int)host.entries().size() * 8 / 10 && renderOK &&
                reactive > probed * 7 / 10;
    printf("── shader test %s ──\n", pass ? "PASSED" : "FAILED");
    glfwDestroyWindow(w);
    glfwTerminate();
    return pass ? 0 : 1;
}
#endif

// Full live chain on a named device: tap → engine → routed output.
// --ping <ch> [<ch>...] [--secs N]: raw full-scale 45 Hz burst on the given
// 1-based UMC1820 outputs, one after another, with the interface's own
// per-output volume/mute state printed so a dead jack can be told from a
// dead amp. Zone routing is bypassed entirely.
static int runPing(int argc, char** argv) {
    std::vector<int> chans;
    float secs = 6.0f;
    for (int i = 0; i < argc; i++) {
        if (std::strcmp(argv[i], "--secs") == 0 && i + 1 < argc) secs = (float)atof(argv[++i]);
        else chans.push_back(atoi(argv[i]));
    }
    auto devs = listOutputDevices();
    const OutDevice* target = nullptr;
    for (auto& d : devs)
        if (d.channels >= 7) { target = &d; break; }
    if (!target) { printf("FAIL: no multichannel interface\n"); return 1; }
    printf("device: %s · %dch @ %.0f Hz\n", target->name.c_str(), target->channels,
           target->sampleRate);
#ifdef __APPLE__
    for (UInt32 el = 0; el <= (UInt32)target->channels; el++) {
        AudioObjectPropertyAddress va = {kAudioDevicePropertyVolumeScalar,
                                         kAudioObjectPropertyScopeOutput, el};
        AudioObjectPropertyAddress ma = {kAudioDevicePropertyMute,
                                         kAudioObjectPropertyScopeOutput, el};
        Float32 v = -1; UInt32 m = 99, sz = sizeof(v);
        bool hv = AudioObjectHasProperty(target->id, &va) &&
                  AudioObjectGetPropertyData(target->id, &va, 0, nullptr, &sz, &v) == noErr;
        sz = sizeof(m);
        bool hm = AudioObjectHasProperty(target->id, &ma) &&
                  AudioObjectGetPropertyData(target->id, &ma, 0, nullptr, &sz, &m) == noErr;
        if (hv || hm)
            printf("  hw out %-2u  vol=%s  mute=%s\n", el, hv ? std::to_string(v).c_str() : "n/a",
                   hm ? (m ? "YES" : "no") : "n/a");
    }
#endif
    Engine eng; eng.init();
    eng.params.masterVolume.store(0.0f); // silence the zone/music chain; ping only
    StereoRing ring; ring.init(48000);
    OutputUnit out;
    if (!out.start(target->id, &eng, &ring)) {
        printf("FAIL output: %s\n", out.lastError.c_str());
        return 1;
    }
    for (int c1 : chans) {
        int c = c1 - 1;
        if (c < 0 || c >= out.activeChannels) { printf("skip ch %d (device has %d)\n", c1, out.activeChannels); continue; }
        printf("PING ch %d for %.0f s ...", c1, secs); fflush(stdout);
        float left = secs;
        float pk = 0;
        while (left > 0) {
            out.ping(c); // 0.9 s bursts back to back
            std::this_thread::sleep_for(std::chrono::milliseconds(850));
            pk = std::max(pk, out.chanPeak[c].load());
            left -= 0.85f;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        printf(" sent peak=%.2f\n", pk);
    }
    out.stop();
    return 0;
}

// --padtest: replicate an octagon click on every ring in silence (PURE patch,
// middle slice) and report the pad frequency + what reaches the interface
static int runPadTest() {
    auto devs = listOutputDevices();
    const OutDevice* target = nullptr;
    for (auto& d : devs) if (d.channels >= 7) { target = &d; break; }
    if (!target) { printf("FAIL: no multichannel interface\n"); return 1; }
    Engine eng; eng.init();
    StereoRing ring; ring.init(48000);
    OutputUnit out;
    if (!out.start(target->id, &eng, &ring)) { printf("FAIL output: %s\n", out.lastError.c_str()); return 1; }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    static const char* zn[NZONES] = {"HEAD", "HEART", "BELLY", "ROOT", "FEET"};
    bool ok = true;
    for (int z = 0; z < NZONES; z++) {
        float hz = std::min(zoneSweetHi(z), std::min(45.0f, eng.zoneHz[z].load()) * std::pow(2.0f, 4 / 8.0f));
        eng.params.padHz[z].store(hz);
        eng.params.padVel[z].store(0.55f + 0.45f * 0.5f);
        eng.params.padGate[z].store(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        int ch = eng.params.zoneChan[z].load(), ch2 = eng.params.zoneChan2[z].load();
        float pk = ch >= 0 ? out.chanPeak[ch].load() : 0;
        float pk2 = ch2 >= 0 ? out.chanPeak[ch2].load() : -1;
        float m = eng.meter[2 + z].load();
        eng.params.padGate[z].store(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bool zok = pk > 0.05f;
        printf("  click %-5s carrier %6.1f Hz -> pad %6.1f Hz  meter=%.2f  ch %d peak=%.3f%s  %s\n",
               zn[z], eng.zoneHz[z].load(), hz, m, ch + 1, pk,
               pk2 >= 0 ? (std::string("  ch ") + std::to_string(ch2 + 1) + " peak=" + std::to_string(pk2).substr(0, 5)).c_str() : "",
               zok ? "OK" : "SILENT");
        ok = ok && zok;
    }
    out.stop();
    return ok ? 0 : 1;
}

// --soundtest: every voice struck on HEART (and full-body ones everywhere),
// then every VFX preset played through the sequencer, checking each produces
// output on the interface. Uses the global engine so the sequencer works.
static int runSoundTest() {
    glfwInit(); // the sequencer clocks off glfwGetTime
    auto devs = listOutputDevices();
    const OutDevice* target = nullptr;
    for (auto& d : devs) if (d.channels >= 7) { target = &d; break; }
    if (!target) { printf("FAIL: no multichannel interface\n"); return 1; }
    gEngine.init();
    gRing.init(48000);
    OutputUnit out;
    if (!out.start(target->id, &gEngine, &gRing)) { printf("FAIL output: %s\n", out.lastError.c_str()); return 1; }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    bool ok = true;
    auto peakZone = [&](int z) { int c = gEngine.params.zoneChan[z].load(); return c >= 0 ? out.chanPeak[c].load() : 0.0f; };
    auto settle = [&](int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); };
    printf("voices:\n");
    for (int p = 0; p < NPATCHES; p++) {
        const PadPatchDef& pd = kPadPatches[p];
        for (int z = 0; z < NZONES; z++) gEngine.params.padGate[z].store(0);
        settle(400);
        gEngine.params.padPatch.store(p);
        midiStrikeZone(HEART, 0.95f, pd.baseHz > 0 ? pd.baseHz : 55.0f, p);
        float pk = 0;
        int ms = pd.atkS > 1.0f ? 2500 : 350;
        for (int t = 0; t < ms; t += 50) { settle(50); pk = std::max(pk, peakZone(HEART)); }
        gEngine.params.padGate[HEART].store(0);
        bool vok = pk > 0.05f;
        printf("  %-10s %-9s peak=%.3f %s\n", pd.name, kPadCategoryNames[pd.cat], pk, vok ? "OK" : "SILENT");
        ok = ok && vok;
    }
    settle(1500);
    printf("vfx:\n");
    buildVfx();
    for (int i = 0; i < (int)gVfx.size(); i++) {
        for (int z = 0; z < NZONES; z++) gEngine.params.padGate[z].store(0);
        settle(400);
        double t0 = glfwGetTime();
        fireVfx(i);
        float pk[NZONES] = {0};
        int zonesHit = 0;
        double len = gVfx[i].length + 0.3;
        while (glfwGetTime() - t0 < len) {
            double now = glfwGetTime();
            float dt = 0.016f;
            advanceSequencer(now);
            for (int z = 0; z < NZONES; z++)
                if (gTestPulse[z] > 0.0f) { gTestPulse[z] -= dt; if (gTestPulse[z] <= 0.0f) gEngine.params.padGate[z].store(0); }
            for (int z = 0; z < NZONES; z++) pk[z] = std::max(pk[z], peakZone(z));
            settle(16);
        }
        for (int z = 0; z < NZONES; z++) if (pk[z] > 0.05f) zonesHit++;
        int zonesScored = 0; { bool seen[NZONES] = {false}; for (auto& st : gVfx[i].steps) seen[st.zone] = true; for (int z = 0; z < NZONES; z++) if (seen[z]) zonesScored++; }
        bool vok = zonesHit >= zonesScored;
        printf("  %-16s %-5s zones %d/%d  peaks H=%.2f He=%.2f B=%.2f R=%.2f F=%.2f %s\n", gVfx[i].name,
               gVfx[i].tuner ? "TUNER" : "VFX", zonesHit, zonesScored, pk[0], pk[1], pk[2], pk[3], pk[4], vok ? "OK" : "MISSED");
        ok = ok && vok;
    }
    out.stop();
    printf("── sound test %s ──\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}

// --splittest [file.wav]: run a stereo track (or a synthetic 3-layer song)
// through both engines offline and report, per zone, the dominant frequency
// and the cross-correlation between zone outputs. Lower correlation =
// the zones are carrying different material. No audio device needed.
static int runSplitTest(const char* wavPath) {
    const int sr = 48000;
    std::vector<float> L, R;
    if (wavPath) {
        FILE* f = fopen(wavPath, "rb");
        if (!f) { printf("FAIL: cannot open %s\n", wavPath); return 1; }
        unsigned char hdr[12]; if (fread(hdr, 1, 12, f) != 12) { fclose(f); return 1; }
        int ch = 2, bits = 16, rate = 48000; long dataLen = 0;
        while (true) {
            unsigned char ck[8]; if (fread(ck, 1, 8, f) != 8) break;
            uint32_t len = ck[4] | (ck[5] << 8) | (ck[6] << 16) | ((uint32_t)ck[7] << 24);
            if (std::memcmp(ck, "fmt ", 4) == 0) {
                unsigned char fm[16]; fread(fm, 1, 16, f);
                ch = fm[2] | (fm[3] << 8); rate = fm[4] | (fm[5] << 8) | (fm[6] << 16) | (fm[7] << 24); bits = fm[14] | (fm[15] << 8);
                fseek(f, len - 16, SEEK_CUR);
            } else if (std::memcmp(ck, "data", 4) == 0) { dataLen = len; break; }
            else fseek(f, len + (len & 1), SEEK_CUR);
        }
        if (dataLen <= 0 || bits != 16) { printf("FAIL: need 16-bit PCM wav\n"); fclose(f); return 1; }
        std::vector<int16_t> pcm(dataLen / 2); fread(pcm.data(), 2, pcm.size(), f); fclose(f);
        long frames = (long)pcm.size() / ch;
        for (long i = 0; i < frames; i++) {
            L.push_back(pcm[i * ch] / 32768.0f);
            R.push_back(pcm[i * ch + (ch > 1 ? 1 : 0)] / 32768.0f);
        }
        printf("track: %s  %ld s  %d ch  %d Hz%s\n", wavPath, frames / rate, ch, rate, rate != sr ? "  (rate mismatch, treated as 48k)" : "");
    } else {
        // synthetic song: bass line (4 notes), kick on beats, a vocal-range
        // melody (6 notes, different rhythm) and a high lead (own rhythm)
        int secs = 24; L.assign((size_t)secs * sr, 0); R = L;
        static const float bassN[4] = {55.0f, 65.4f, 73.4f, 49.0f};
        static const float vocN[6] = {329.6f, 392.0f, 440.0f, 349.2f, 293.7f, 392.0f};
        static const float leadN[5] = {1318.5f, 1568.0f, 1174.7f, 1760.0f, 1046.5f};
        double pb = 0, pv = 0, pl = 0;
        for (long i = 0; i < (long)L.size(); i++) {
            double t = (double)i / sr;
            float bass = std::sin(pb) * 0.5f; pb += 2 * M_PI * bassN[(int)(t / 2.0) % 4] / sr;
            float beat = std::fmod(t, 0.5);
            float kick = std::sin(2 * M_PI * (50 + 80 * std::exp(-beat * 30)) * beat) * std::exp(-beat * 18) * 0.8f;
            float vn = vocN[(int)(t / 0.75) % 6]; float ve = 0.5f + 0.5f * std::sin(2 * M_PI * 1.3333 * t);
            float voc = std::sin(pv) * 0.3f * ve; pv += 2 * M_PI * vn / sr;
            float ln = leadN[(int)(t / 0.4) % 5]; float le = std::fmod(t, 0.4) < 0.2 ? 1.0f : 0.15f;
            float lead = std::sin(pl) * 0.2f * le; pl += 2 * M_PI * ln / sr;
            L[i] = bass + kick + voc * 0.9f + lead * 1.1f;
            R[i] = bass + kick + voc * 1.1f + lead * 0.9f;
        }
        printf("track: synthetic 24 s (bass line · kick · vocal melody · lead)\n");
    }
    static const char* zn[NZONES] = {"HEAD", "HEART", "BELLY", "ROOT", "FEET"};
    static const char* modeName[5] = {"SYNTH", "SPLIT", "MONO", "STEREO", "BODY"};
    for (int mode = 0; mode < 5; mode++) {
        Engine eng; eng.init();
        eng.params.engineMode.store(mode);
        const int blk = 512;
        std::vector<float> zbuf[NZONES]; float* vib[NZONES];
        for (int z = 0; z < NZONES; z++) { zbuf[z].resize(blk); vib[z] = zbuf[z].data(); }
        long total = (long)L.size(), skip = std::min<long>(total / 3, 6L * sr); // let AGC/warmup settle
        std::vector<float> outZ[NZONES];
        for (long pos = 0; pos + blk <= total; pos += blk) {
            eng.process(&L[pos], &R[pos], vib, blk);
            if (pos >= skip) for (int z = 0; z < NZONES; z++) outZ[z].insert(outZ[z].end(), zbuf[z].begin(), zbuf[z].end());
        }
        printf("── %s ──\n", modeName[mode]);
        long N = (long)outZ[0].size();
        float rms[NZONES], domHz[NZONES];
        for (int z = 0; z < NZONES; z++) {
            double e = 0; for (long i = 0; i < N; i++) e += outZ[z][i] * outZ[z][i];
            rms[z] = std::sqrt(e / std::max<long>(N, 1));
            // dominant frequency by zero-crossing rate over the whole take
            long zc = 0; for (long i = 1; i < N; i++) if ((outZ[z][i] >= 0) != (outZ[z][i - 1] >= 0)) zc++;
            domHz[z] = 0.5f * zc * sr / (float)std::max<long>(N, 1);
        }
        // pitch variety: how many distinct notes each zone plays (0.5 s windows)
        for (int z = 0; z < NZONES; z++) {
            std::vector<int> notes; int win = sr / 2;
            for (long w0 = 0; w0 + win <= N; w0 += win) {
                long zc = 0; double e = 0;
                for (long i = w0 + 1; i < w0 + win; i++) { if ((outZ[z][i] >= 0) != (outZ[z][i - 1] >= 0)) zc++; e += outZ[z][i] * outZ[z][i]; }
                if (std::sqrt(e / win) < 0.02f) continue;
                float hz = 0.5f * zc * sr / (float)win;
                int note = (int)std::lround(12.0f * std::log2(std::max(hz, 20.0f) / 55.0f));
                if (std::find(notes.begin(), notes.end(), note) == notes.end()) notes.push_back(note);
            }
            printf("  %-6s rms=%.3f  dominant %5.1f Hz  distinct notes=%d\n", zn[z], rms[z], domHz[z], (int)notes.size());
        }
        // envelope correlation between zones (50 ms RMS envelopes): 1 = same rhythm
        int ew = sr / 20; long M = N / ew;
        std::vector<float> env[NZONES];
        for (int z = 0; z < NZONES; z++) for (long m = 0; m < M; m++) {
            double e = 0; for (int i = 0; i < ew; i++) e += outZ[z][m * ew + i] * outZ[z][m * ew + i];
            env[z].push_back(std::sqrt(e / ew));
        }
        auto corr = [&](int a, int b) {
            double ma = 0, mb = 0; for (long m = 0; m < M; m++) { ma += env[a][m]; mb += env[b][m]; } ma /= M; mb /= M;
            double sab = 0, saa = 0, sbb = 0;
            for (long m = 0; m < M; m++) { double da = env[a][m] - ma, db = env[b][m] - mb; sab += da * db; saa += da * da; sbb += db * db; }
            return saa > 0 && sbb > 0 ? sab / std::sqrt(saa * sbb) : 1.0;
        };
        double sum = 0; int cnt = 0;
        printf("  rhythm correlation:");
        for (int a = 0; a < NZONES; a++) for (int b = a + 1; b < NZONES; b++) { double c = corr(a, b); sum += c; cnt++; }
        printf("  mean %.2f  (HEAD-FEET %.2f · HEART-ROOT %.2f · BELLY-HEART %.2f)\n", sum / cnt, corr(HEAD, FEET), corr(HEART, ROOT), corr(BELLY, HEART));
    }
    return 0;
}

static int runRouteTest(const char* devSub) {
    printf("── route test: \"%s\" ──\n", devSub);
    auto devs = listOutputDevices();
    const OutDevice* target = nullptr;
    for (auto& d : devs)
        if (d.name.find(devSub) != std::string::npos) { target = &d; break; }
    if (!target) { printf("FAIL: no output device matching \"%s\"\n", devSub); return 1; }
    printf("device: %s · %dch @ %.0f Hz\n", target->name.c_str(), target->channels,
           target->sampleRate);

    Engine eng; eng.init();
    eng.params.engineMode.store(0); // hardware check on the tonal engine (a test tone has no drum layer)
    StereoRing ring; ring.init(48000);
    SystemTap tap;
    if (!tap.start(&ring)) { printf("FAIL tap: %s\n", tap.lastError.c_str()); return 1; }

    OutputUnit out;
    if (!out.start(target->id, &eng, &ring)) {
        printf("FAIL output: %s\n", out.lastError.c_str());
        tap.stop();
        return 1;
    }
    printf("output running: %d channels active\n", out.activeChannels);
    printf("routing: music → ch %d-%d · zones → ch %d(+%d)/%d/%d/%d/%d\n",
           eng.params.musicChanL.load() + 1, eng.params.musicChanL.load() + 2,
           eng.params.zoneChan[0].load() + 1, eng.params.zoneChan2[0].load() + 1,
           eng.params.zoneChan[1].load() + 1,
           eng.params.zoneChan[2].load() + 1, eng.params.zoneChan[3].load() + 1,
           eng.params.zoneChan[4].load() + 1);

    { std::string tone = selftestTonePath();
      writeToneWav(tone.c_str(), 440.0f, 3.0f);
      playToneAsync(tone); }
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    float inPeak = tap.inputPeak.load();
    float zonePk[NZONES];
    for (int z = 0; z < NZONES; z++) zonePk[z] = eng.meter[2 + z].load();
    printf("live: input peak=%.4f  zone meters H=%.2f He=%.2f B=%.2f R=%.2f F=%.2f\n",
           inPeak, zonePk[0], zonePk[1], zonePk[2], zonePk[3], zonePk[4]);
    bool ok = inPeak > 0.005f;
    for (int z = 0; z < NZONES; z++) ok = ok && zonePk[z] > 0.01f;

    // ring isolation: solo each zone in turn with a pulse and confirm only its
    // interface channel carries signal while the tone keeps feeding the engine
    printf("solo pass (music still playing):\n");
    { std::string tone = selftestTonePath();
      writeToneWav(tone.c_str(), 440.0f, 6.0f);
      playToneAsync(tone); }
    static const char* zn[NZONES] = {"HEAD", "HEART", "BELLY", "ROOT", "FEET"};
    for (int z = 0; z < NZONES; z++) {
        eng.params.soloZone.store(z);
        eng.params.padHz[z].store(55.0f);
        eng.params.padVel[z].store(1.0f);
        eng.params.padGate[z].store(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
        float mine = 0, leak = 0;
        int ch = eng.params.zoneChan[z].load();
        int ch2 = eng.params.zoneChan2[z].load();
        if (ch >= 0) mine = out.chanPeak[ch].load();
        if (ch2 >= 0) mine = std::min(mine, out.chanPeak[ch2].load()); // both sends must carry it
        for (int o = 0; o < NZONES; o++) {
            int oc = eng.params.zoneChan[o].load(), oc2 = eng.params.zoneChan2[o].load();
            if (o != z && oc >= 0) leak = std::max(leak, out.chanPeak[oc].load());
            if (o != z && oc2 >= 0) leak = std::max(leak, out.chanPeak[oc2].load());
        }
        eng.params.padGate[z].store(0);
        bool zok = mine > 0.01f && leak < 0.002f;
        if (ch2 >= 0)
            printf("  solo %-5s ch %d+%d peak=%.4f  others max=%.5f  %s\n", zn[z], ch + 1,
                   ch2 + 1, mine, leak, zok ? "PASS" : "FAIL");
        else
            printf("  solo %-5s ch %d peak=%.4f  others max=%.5f  %s\n", zn[z], ch + 1, mine,
                   leak, zok ? "PASS" : "FAIL");
        ok = ok && zok;
    }
    eng.params.soloZone.store(-1);
    // channel finder: raw ping on the last device channel must reach only it
    {
        int pc = out.activeChannels - 1;
        out.ping(pc);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        float mine = out.chanPeak[pc].load();
        printf("ping ch %d peak=%.4f %s\n", pc + 1, mine, mine > 0.3f ? "PASS" : "FAIL");
        ok = ok && mine > 0.3f;
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
    }
    out.stop(); tap.stop();
    printf("── route test %s ──\n", ok ? "PASSED" : "FAILED");
    return ok ? 0 : 1;
}
