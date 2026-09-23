# AVA OS

Live vibroacoustic engine for the AVA octagon at [Audio Visual Artifacts](https://www.avartifacts.com), Greenpoint, Brooklyn.

AVA OS listens to whatever the computer is playing and turns it into five body-zone vibration channels in real time, HEAD, HEART, BELLY, ROOT and FEET, plus the stereo music feed, all on one multichannel audio interface. The octagon on screen is also a playable pad controller, a tuner, and a MIDI controller, with 200+ audio-reactive shader backgrounds for a projector.

**Download:** [avartifacts.com/ava-os](https://www.avartifacts.com/ava-os.html) · macOS (Apple silicon) and Windows builds on the [releases page](../../releases).

## What it does

- **System audio in.** macOS: a global process tap (macOS 14.4+). Windows: WASAPI loopback of the default output.
- **Engine.** The bed is tuned to the music's lowest pitch, doubled: a time-domain tracker (YIN on the bass band) measures the fundamental of the lowest note sounding, the rings play 2·f0 octave-folded into 40-80 Hz and the feet carry the fundamental itself. Pulses come from a sample-accurate low-end transient detector (kick / bass pluck), tempo from spectral-flux autocorrelation. On top: breath-following dynamics, brainwave entrainment pulses (delta to gamma), body sweeps and zone accents. `src/engine.cpp`, `src/analysis.cpp`.
- **Pads.** Click, drag or MIDI-strike any ring. Voices: PURE, WHALE, QUAKE, HEART, PURR, DROP. Press-and-hold swells.
- **Routing.** Music pair, optional surround pair, one or two output channels per zone, per-zone trim, solo one ring.
- **HEALTH.** Interface status, per-channel output meters, TEST ALL ZONES sweep, ISOLATE RING solo, PING any jack, transducer thermal guard.
- **TUNER.** Five zone orbs on a 20 to 200 Hz ruler, just-intonation presets, drones, MIDI out as the "AVA OS Octagon" virtual port (macOS).
- **Visuals.** Shader library in `shaders/` (ISF-style fragment shaders with `manifest.json`), audio-reactive, send-to-display for a second screen.

Default routing on a Behringer UMC1820:

| Zone | Output |
|---|---|
| Music / speakers | 1-2 |
| FEET (center pad) | 4 |
| BELLY | 5 |
| ROOT | 6 |
| HEART | 7 |
| HEAD | 9 + 10 |

## Build

Requirements: CMake 3.20+, a C++17 compiler. Dependencies (GLFW, Dear ImGui, nlohmann/json, miniaudio) are fetched by CMake.

macOS (Apple silicon, macOS 14.4 SDK or newer):

```
cmake -B build
cmake --build build -j8
open "build/AVA OS.app"
```

Windows, cross-compiled from macOS or Linux with mingw-w64:

```
cmake -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win -j8
```

Packaging: `package.sh` (macOS zip) and `package-win.sh` (Windows zip with `shaders/` next to the exe).

## Command-line checks

All run headless against the first interface with 7+ outputs.

```
"AVA OS" --selftest              # engine + system-audio tap
"AVA OS" --routetest UMC1820     # every zone reaches its channel, solo leaks nothing
"AVA OS" --padtest               # replicate an octagon click on each ring
"AVA OS" --ping 9 10 --secs 8    # raw 45 Hz burst on given outputs (find a jack by feel)
```

## Layout

```
src/main.cpp        UI (Dear ImGui), octagon, tuner, health, MIDI map, CLI tests
src/engine.*        vibroacoustic engine, pad synth, FX bus
src/analysis.*      lowest-pitch (YIN) / onset / tempo / key analysis
src/audio_mix.cpp   shared output mixer: routing, solo, thermal guard, ping, meters
src/audio_out.*     macOS CoreAudio output      src/audio_win.cpp  Windows miniaudio output
src/capture_tap.*   macOS system-audio tap      src/midi_in.mm, midi_out.mm  CoreMIDI
src/shaderhost.*    shader background renderer
shaders/            shader library + manifest.json
ipad/               iPad port (audio only)
```

## Notes for agents

Everything the app needs is in this repo. Settings live in `~/Library/Application Support/AVA OS` on macOS and next to the exe on Windows. There is no network access. The macOS build is ad-hoc signed; see the download page for the Gatekeeper steps.
