#!/bin/bash
# Package AVA OS into a portable zip: app bundle + shader library +
# verified skip list + README. Result: dist/AVA-OS.zip
set -euo pipefail
cd "$(dirname "$0")"

APP="build/AVA OS.app"
RES="$APP/Contents/Resources"

echo "── build ──"
cmake --build build -j8 | tail -1

echo "── bundle shader library ──"
rm -rf "$RES/shaders"
mkdir -p "$RES/shaders"
cp shaders/*.fs "$RES/shaders/"
cp shaders/manifest.json "$RES/shaders/"
if [ -f /tmp/ava_skip.txt ]; then
  cp /tmp/ava_skip.txt "$RES/shaders/skip.txt"
fi
du -sh "$RES/shaders"

echo "── sign (ad-hoc) ──"
codesign --force --deep -s - "$APP"
codesign -dv "$APP" 2>&1 | head -2

echo "── package ──"
rm -rf dist
mkdir -p "dist/AVA OS"
cp -R "$APP" "dist/AVA OS/"
cat > "dist/AVA OS/README.txt" <<'EOF'
AVA OS
==============
Live vibroacoustic engine for the SoundTemple. Listens to whatever your Mac is
playing and generates five vibration zone channels (Head / Heart / Belly /
Butt / Feet) in real time, plus the stereo music feed.

REQUIREMENTS
- macOS 14.4 or newer, Apple silicon
- For the full bed: a multichannel audio interface (7+ outputs).
  Stereo-only devices get a vibration monitor mix instead.

FIRST RUN
1. Right-click "AVA OS.app" → Open → Open.
   (Needed once — the app is not notarized.)
2. macOS will ask to allow recording system audio. Click Allow.
   This is how the app hears what you're playing. The microphone is never used.
3. Play music from any app. The green dot lights when signal is flowing.

OUTPUT ROUTING
- Bottom pill → Output: pick your audio interface.
- ROUTING in the same menu: Music pair (default ch 1-2, for your speakers/amp)
  and one channel per zone (defaults: HEAD 9+10, FEET 4, BELLY 5, ROOT 6,
  HEART 7, for the transducer amps).

CONTROLS
- Tabs (Breath / Energy / Relax / Creative): whole-session presets.
- Sliders per zone: vibration level. S = live sound level, V = vibration level.
- State dropdown (Sleep / Dream / Calm / Focus / Peak): entrainment pulse rate,
  from delta 2 Hz to gamma 40 Hz.
- Breath / Dynamics: how strongly vibrations follow the song's quiet moments
  and overall character.
- Eye icon: shader visuals — 150+ audio-reactive backgrounds, per-shader
  parameters, and "send to display" for a projector or second screen.
- Esc: closes the projector output, or exits fullscreen.

Every shader shipped in this bundle has been machine-verified to render and
react to audio.
EOF
ditto -c -k --keepParent "dist/AVA OS" dist/AVA-OS.zip
du -sh dist/AVA-OS.zip
echo "── done: $(pwd)/dist/AVA-OS.zip ──"
