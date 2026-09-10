#!/bin/bash
# Package the Windows build into a portable zip: exe + shader library + README.
# Result: dist/AVA-OS-Windows.zip
set -euo pipefail
cd "$(dirname "$0")"

echo "── build (mingw-w64 cross) ──"
cmake --build build-win -j8 | tail -1

OUT="dist/AVA OS Windows"
rm -rf "$OUT" dist/AVA-OS-Windows.zip
mkdir -p "$OUT/shaders"

cp "build-win/AVA OS.exe" "$OUT/"

echo "── bundle shader library ──"
cp shaders/*.fs "$OUT/shaders/"
cp shaders/manifest.json "$OUT/shaders/"
if [ -f /tmp/ava_skip.txt ]; then
  cp /tmp/ava_skip.txt "$OUT/shaders/skip.txt"
fi
du -sh "$OUT/shaders"

cat > "$OUT/README.txt" <<'EOF'
AVA OS — WINDOWS
========================
Live vibroacoustic engine for the SoundTemple. Listens to whatever your PC is
playing and generates five vibration zone channels (Head / Heart / Belly /
Butt / Feet) in real time, plus the stereo music feed. The octagon is also a
playable pad controller with a voice library (PURE / WHALE / QUAKE / HEART /
PURR / DROP) — click and hold the rings to play vibrations directly.

REQUIREMENTS
- Windows 10 or newer, 64-bit.
- For the full bed: a multichannel audio interface (7+ outputs), e.g. the
  Behringer UMC1820 with its Windows driver installed.
  Stereo-only devices get a vibration monitor mix instead.

FIRST RUN
1. Unzip the whole folder anywhere (keep "shaders" next to the exe).
2. Double-click "AVA OS.exe". If SmartScreen warns, click
   "More info" then "Run anyway" (the app is unsigned).
3. Play music from any app. The green dot lights when signal is flowing.

IMPORTANT — AUDIO SETUP
The app captures what Windows plays on the DEFAULT output device (WASAPI
loopback). Keep your speakers/headphones as the Windows default output and
pick your multichannel interface inside the app (bottom pill → Output).
If you make the interface itself the Windows default, the app would capture
its own output and feed back.

OUTPUT ROUTING
- Bottom pill → Output: pick your audio interface.
- ROUTING in the same menu: Music pair (default ch 1-2) and one channel per
  zone (defaults: HEAD 9+10, FEET 4, BELLY 5, ROOT 6, HEART 7).

CONTROLS
- Octagon: click/hold rings to PLAY vibration pads. Drag to glide. The 8
  slices step pitch around each ring; pressing nearer the outer edge hits
  harder. Right-click a ring to mute/unmute that zone. The music-note button
  at the left edge picks the pad voice — WHALE is the full-body surge (hold
  to build), QUAKE is the violent shake.
- TUNER (button next to the AVA OS wordmark): five zone orbs on a 20-200 Hz
  ruler. Drag an orb to retune that zone (Shift = fine), right-click or keys
  1-5 = DRONE (hold it sounding), Esc releases. Numbers between orbs are the
  beat rate you feel where two zones overlap. Tuning presets: BODY CHORD,
  WELL-TUNED, BEAT 3, UNISON. Saved to ava_tuner.txt next to the exe.
- HEALTH (top right): interface status, per-channel output meters, MAX ALL
  LEVELS and TEST ALL ZONES (sweeps one pulse per transducer).
- MIDI in/out (pad controllers, the "AVA OS Octagon" virtual port) is
  macOS-only in this build; the TUNER's MIDI OUT row shows PORT UNAVAILABLE.
- Tabs (presets), per-zone sliders, State dropdown, Breath / Dynamics: as on
  the Mac build.
- Eye icon: shader visuals, per-shader parameters, "send to display" for a
  projector, and the Edge fade slider (black vignette on the output).
- Esc: closes the projector output, or exits fullscreen.
EOF

echo "── package ──"
(cd dist && zip -qr AVA-OS-Windows.zip "AVA OS Windows")
du -sh dist/AVA-OS-Windows.zip
echo "── done: $(pwd)/dist/AVA-OS-Windows.zip ──"
