# AVA Stem Spec — composing for the bed

AVA OS drives a five-zone vibroacoustic bed (HEAD · HEART · BELLY · BUTT · FEET) plus a stereo music pair. Normally it listens to whatever is playing and derives the vibration itself. This spec is for the other case: **you write the vibration**, one track per zone, and AVA plays it exactly as authored.

There are two ways in. Both use the same seven-channel layout.

| Channel | Carries | Notes |
|---|---|---|
| 1-2 | Music (stereo) | Goes to the music pair, heard on speakers or headphones |
| 3 | HEAD | Ring behind the head |
| 4 | HEART | Ring under the chest |
| 5 | BELLY | Ring under the belly |
| 6 | BUTT | Ring under the hips (also called ROOT) |
| 7 | FEET | Centre pad under the feet (ButtKicker class transducer) |

## 1. Stem set (files)

A folder of WAV files, one per channel. Drop the folder on the AVA OS window, or Artifact › Play › Load set folder.

**Format.** 48 kHz. 16, 24 or 32-bit PCM or 32-bit float. Mono or stereo (zone stems are summed to mono). Any other sample rate is refused with an error, so export at 48 kHz.

**Names.** Files are matched by name, case-insensitive. Either convention works:

- `Piece_ch1+2_hp.wav`, `Piece_ch3_HEAD.wav`, `Piece_ch4_HEART.wav`, `Piece_ch5_BELLY.wav`, `Piece_ch6_ROOT.wav`, `Piece_ch7_FEET.wav`
- or any file containing `head`, `heart`, `belly`, `root` or `butt`, `feet`, and `master`, `hp`, `main` or `music` for the stereo track. (`heart` is checked before `head`.)

**Partial sets.** A set may bring only some zones. Zones without a stem are filled by the live engine from the set's music track, aligned to the stems. Author only the layers you care about.

**Alignment.** All stems must start at the same sample. Leading silence below −60 dBFS is skipped automatically, as long as it is the same length in every file (export the whole arrangement from bar 1).

**Levels.** Zone stems are played as-is through the per-zone trim, the thermal guard and the output limiter. Leave headroom: peaks at −6 dBFS, long holds around −12 dBFS. A stem that sits at full scale for a long time will be eased down by the thermal guard, which protects the voice coils but changes your dynamics.

## 2. Live set (DAW straight into AVA)

While composing, skip the export. Route the DAW's output to a multichannel virtual device, and AVA takes it live.

1. Install BlackHole 16ch (free) or use your interface's loopback.
2. In the DAW, set the audio output to that device. Put the music on outputs 1-2 and each zone track on 3-7.
3. In AVA OS: Artifact › Play › LIVE SET. Pick the device, press ON. The meters show what arrives on each channel.
4. Click any zone chip to hand it back to the engine. That zone then follows your music while you work on the others.

Latency is the device's buffer plus about 3 ms. Zones left to the engine are aligned to your authored ones by the sync look-ahead.

## 3. What a zone can feel

Transducers are not speakers. What reaches the body is roughly:

| Zone | Hardware band | Sweet band | Feels like |
|---|---|---|---|
| HEAD, HEART, BELLY, BUTT (rings) | 10-80 Hz | 30-70 Hz | 30-45 Hz deep hum, 50-70 Hz firm buzz |
| FEET (centre pad) | 5-200 Hz | 20-80 Hz | 15-30 Hz slow thump, 40-60 Hz push |

- Above about 110 Hz almost nothing is felt. Melodic content needs to be folded down an octave or two.
- Below 15 Hz the rings do not move. Sub-bass goes to FEET.
- Sine and triangle waves read cleanly. Noise reads as texture. Hard square edges read as rattle.
- Amplitude is the main expressive dimension. A slow swell over 4-10 seconds is felt as breathing. Pulses at 1-3 Hz read as heartbeat, at 6-10 Hz as flutter, at 40 Hz as a steady hum (the classic vibroacoustic frequency).
- The body adapts within a minute to a constant level. Leave silence. Negative space is the strongest effect you have.

## 4. Ableton template

Set up once, reuse per piece:

1. Audio preferences: 48 kHz. Output device: your interface (or BlackHole 16ch for live work).
2. Create five Return tracks named `A-HEAD`, `B-HEART`, `C-BELLY`, `D-BUTT`, `E-FEET`. Set each return's output to Ext. Out 3, 4, 5, 6, 7.
3. Your music tracks go to Master → Ext. Out 1/2.
4. Zone material lives on ordinary tracks set to "Sends Only", sent 100 % to one return each. Or put the zone audio directly on the return's input.
5. Export: select the five returns plus Master → File › Export Audio/Video → Rendered Track = Selected Tracks Only, 48 kHz, 24-bit WAV, Normalize off, range = the whole arrangement.

The export names the files `Piece A-HEAD.wav`, `Piece Master.wav` and so on. AVA matches them by the zone word.

## 5. Checklist before you hand over a set

- [ ] 48 kHz, all files the same length
- [ ] one file per zone you want to author, named with the zone word
- [ ] one stereo music file named master, hp, main or music
- [ ] zone content between 15 and 110 Hz, peaks around −6 dBFS
- [ ] drop the folder on AVA OS, press Play, watch the six meters
