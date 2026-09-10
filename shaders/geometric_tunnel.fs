/*{
  "CATEGORIES": [
    "Generator",
    "Tunnel",
    "Audio Reactive"
  ],
  "DESCRIPTION": "Geometric Tunnel — architectural recession to a vanishing point, rendered in screen-space with log-distance mapping. Five mood presets reference Kubrick's Stargate (Trumbull '68), Anthony McCall's solid-light cones, Escher tessellated stairwells, Bauhaus concentric primaries (Klee/Kandinsky), and brutalist concrete grids. Bass pushes forward speed, mid drives lateral handheld sway, treble sparkles the off-center vanishing-point hot spot. Stays alive in silence with constant slow forward drift. Single-pass, LINEAR HDR.",
  "INPUTS": [
    {
      "NAME": "mood",
      "LABEL": "Mood",
      "TYPE": "long",
      "DEFAULT": 0,
      "VALUES": [
        0,
        1,
        2,
        3,
        4
      ],
      "LABELS": [
        "Stargate",
        "McCall Cone",
        "Escher Stair",
        "Bauhaus",
        "Brutalist"
      ]
    },
    {
      "NAME": "hazeDensity",
      "LABEL": "Atmosphere",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.55
    },
    {
      "NAME": "exposure",
      "LABEL": "Exposure",
      "TYPE": "float",
      "MIN": 0.2,
      "MAX": 2.5,
      "DEFAULT": 1
    },
    {
      "NAME": "tunnelSpeed",
      "LABEL": "Forward Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 0.55,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "tunnelTwist",
      "LABEL": "Twist",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.18,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "hueShift",
      "LABEL": "Hue Shift",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0,
      "GROUP": "Color"
    },
    {
      "NAME": "colorBoost",
      "LABEL": "Color Boost",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 1,
      "GROUP": "Color"
    },
    {
      "NAME": "vanishOffX",
      "LABEL": "Vanish Off-X",
      "TYPE": "float",
      "MIN": -0.4,
      "MAX": 0.4,
      "DEFAULT": 0.08,
      "GROUP": "Camera / Layout"
    },
    {
      "NAME": "vanishOffY",
      "LABEL": "Vanish Off-Y",
      "TYPE": "float",
      "MIN": -0.4,
      "MAX": 0.4,
      "DEFAULT": -0.06,
      "GROUP": "Camera / Layout"
    },
    {
      "NAME": "bgColor",
      "LABEL": "Background",
      "TYPE": "color",
      "DEFAULT": [
        0,
        0,
        0,
        0
      ],
      "GROUP": "Background"
    },
    {
      "NAME": "audioReact",
      "LABEL": "Audio React",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 1,
      "GROUP": "Audio Reactivity"
    }
  ]
}*/

// ════════════════════════════════════════════════════════════════════════
//  Geometric Tunnel — five compositional moods, all keyed to a single
//  log-distance screen-space mapping:
//        depth = log(1/r)   so depth grows toward the vanishing point
//        angle = atan2(uv - vanishingPoint)
//  Decorations live at integer depths via fract(depth + flightZ) so the
//  tunnel "flies past" without raymarching. The vanishing point is held
//  off-center for compositional ease (rule of thirds nudge).
//
//  Output is LINEAR HDR — no internal tonemap. Host applies ACES.
//  Vanishing-point peaks 1.6+, scan-band crests ~1.4 to seed bloom.
// ════════════════════════════════════════════════════════════════════════

#define PI   3.14159265359
#define TAU  6.28318530718

// ─── tiny utils ───────────────────────────────────────────────────────
float hash11(float n) { return fract(sin(n * 12.9898) * 43758.5453); }
float hash21(vec2 p)  { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

// Smooth pulse around a center value
float band(float x, float center, float w) {
    return smoothstep(w, 0.0, abs(x - center));
}

// Recession factor: 1.0 right at the vanishing point, 0.0 at the frame edge.
// Drives both depth darkening (near = dim) and vanishing-point glow (far = bright).
float recession(float r) {
    return 1.0 - smoothstep(0.0, 0.9, r);
}

// ─── MOOD 0 : Stargate — vertical rainbow bands flying past ───────────
vec3 moodStargate(float depth, float angle, float r, float flightZ, float audio, float treble) {
    // Hue sweeps with depth (so bands fly toward camera) and with angle
    // (so columns colour-shift around the wall).
    float bandIdx = angle * (3.0 / PI) * 6.0 + depth * 1.6 + flightZ * 0.6;
    float h       = fract(bandIdx * 0.08 + flightZ * 0.05);
    vec3  rb      = hsv2rgb(vec3(h, 0.95, 1.0));

    // Hard-edged saturated columns marching past the vanishing point
    float colJit  = step(0.55, fract(bandIdx * 0.5));
    rb            = mix(rb * 0.35, rb * 1.4, colJit);

    // Iconic horizontal scan-band: a ring at a flying depth, crossing radially
    float ringPos = fract(depth * 0.5 + flightZ * 0.35);
    float ring    = exp(-pow((ringPos - 0.5) * 8.0, 2.0));
    rb           += vec3(1.4, 1.1, 0.8) * ring * (0.7 + 0.7 * treble);

    // Recession: near (large r) is dim, far (small r) is bright
    float rec     = recession(r);
    rb           *= mix(0.08, 1.2, rec);

    // Bright vanishing-point spike — peaks above 1.6 for bloom
    rb           += vec3(1.7, 1.5, 2.0) * pow(rec, 6.0) * (0.7 + 0.6 * audio);
    return rb;
}

// ─── MOOD 1 : McCall Cone — single cone of light through fog ──────────
vec3 moodMcCall(float depth, float angle, float r, float flightZ, float audio) {
    // Volumetric reading: brightness falls with radial distance from the
    // vanishing point, plus a soft "haze" gradient that boils slowly.
    float core    = exp(-r * 7.0);                 // tight on-axis core
    float halo    = exp(-r * 2.0) * 0.55;          // wider halo
    float fogBoil = 0.5 + 0.5 * sin(flightZ * 1.3 + angle * 3.0);
    float haze    = exp(-r * 1.1) * (0.30 + 0.40 * fogBoil);

    // Solid-light striations radiating from the vanishing point
    float rays    = pow(0.5 + 0.5 * cos(angle * 9.0 + flightZ * 0.5), 12.0);
    rays         *= exp(-r * 1.8);

    // Depth-banded volumetric slabs (the Anthony McCall reading)
    float slab    = 0.5 + 0.5 * sin(depth * 3.0 - flightZ * 1.4);
    slab          = pow(slab, 4.0) * exp(-r * 2.2);

    vec3 light    = vec3(0.95, 0.92, 1.00);        // cool white lamp
    vec3 col      = light * (core * 1.7 + halo + haze * 0.8 + rays * 0.7 + slab * 0.6);
    col          *= 0.9 + 0.4 * audio;             // breath
    return col;
}

// ─── MOOD 2 : Escher Stair — tessellated polygons along walls ─────────
vec3 moodEscher(float depth, float angle, float r, float flightZ, float audio) {
    // Subdivide the tunnel wall into a (depth, theta) grid; each cell hosts
    // a hexagon whose orientation flips with cell parity for the
    // impossible-stair illusion.
    float zCell   = floor(depth * 1.6 + flightZ);
    float aCell   = floor(angle * 6.0 / PI + zCell * 0.5); // shift each row
    vec2  cellId  = vec2(aCell, zCell);

    // Local cell coords [-1,1]
    float zLocal  = fract(depth * 1.6 + flightZ) * 2.0 - 1.0;
    float aLocal  = fract(angle * 6.0 / PI + zCell * 0.5) * 2.0 - 1.0;
    vec2  q       = vec2(aLocal, zLocal);

    // Rotate alternate cells 30° for the woven Escher reading
    float rot     = mod(cellId.x + cellId.y, 2.0) * (PI / 6.0);
    float c = cos(rot), s = sin(rot);
    q             = mat2(c, -s, s, c) * q;

    // Hexagon SDF
    vec2  hq      = abs(q);
    float hex     = max(hq.x * 0.866 + hq.y * 0.5, hq.y) - 0.78;

    // Edge stroke + fill
    float stroke  = smoothstep(0.06, 0.02, abs(hex));
    float fill    = smoothstep(0.02, -0.06, hex);

    // Two-tone weave — alternates per cell to read as Escher
    float tone    = mod(cellId.x + cellId.y * 0.5, 2.0);
    vec3  warm    = vec3(0.95, 0.78, 0.42);
    vec3  cool    = vec3(0.28, 0.42, 0.66);
    vec3  fillCol = mix(cool, warm, tone) * 0.85;
    vec3  edgeCol = vec3(1.0, 0.92, 0.72) * 1.5;

    vec3 col      = fillCol * fill + edgeCol * stroke;

    // Recession: dim the near rim, glow the vanishing point
    float rec     = recession(r);
    col          *= mix(0.10, 1.1, rec);
    col          += vec3(1.6, 1.3, 0.85) * pow(rec, 5.0) * (0.5 + 0.5 * audio);
    return col;
}

// ─── MOOD 3 : Bauhaus — concentric primary frames spaced along axis ───
vec3 moodBauhaus(float depth, float angle, float r, float flightZ, float audio) {
    // Each integer step in depth places one frame; cycles 5 primaries / 3 shapes.
    float zCell  = floor(depth * 0.8 + flightZ);
    float zLocal = fract(depth * 0.8 + flightZ);
    float framePresence = exp(-pow((zLocal - 0.5) * 4.5, 2.0));

    // Klee/Kandinsky primaries: red, yellow, blue, black, off-white
    int idx = int(mod(zCell, 5.0));
    vec3 cFrame;
    if      (idx == 0) cFrame = vec3(1.10, 0.22, 0.18);
    else if (idx == 1) cFrame = vec3(1.15, 0.92, 0.16);
    else if (idx == 2) cFrame = vec3(0.10, 0.36, 0.95);
    else if (idx == 3) cFrame = vec3(0.06, 0.06, 0.06);
    else               cFrame = vec3(1.05, 1.00, 0.92);

    // Shape per cell: 0 = ring, 1 = filled disk, 2 = square frame
    float shapeIdx = mod(zCell, 3.0);
    float shape;
    if (shapeIdx < 0.5) {
        // Ring
        shape = band(r, 0.55, 0.04);
    } else if (shapeIdx < 1.5) {
        // Filled disk with a thin outline accent
        float disk    = smoothstep(0.50, 0.46, r);
        float outline = band(r, 0.50, 0.015) * 1.3;
        shape = disk * 0.7 + outline;
    } else {
        // Square frame (in angle/r space, approximated by max(|cos|,|sin|))
        float sq    = max(abs(r * cos(angle)), abs(r * sin(angle)));
        shape = band(sq, 0.50, 0.035);
    }

    vec3 col = cFrame * shape * framePresence * 1.5;

    // Recession + vanishing pop
    float rec = recession(r);
    col      *= mix(0.18, 1.0, rec);
    col      += vec3(1.6, 1.4, 0.95) * pow(rec, 6.0) * (0.5 + 0.5 * audio);
    return col;
}

// ─── MOOD 4 : Brutalist — gridded concrete panels, cool ambient end ───
vec3 moodBrutalist(float depth, float angle, float r, float flightZ, float audio) {
    // Panel grid in (depth, theta). Seams = bright lines between cells.
    float zCell   = floor(depth * 2.2 + flightZ);
    float aCell   = floor(angle * 8.0 / PI);
    float zLocal  = fract(depth * 2.2 + flightZ);
    float aLocal  = fract(angle * 8.0 / PI);

    // Seams: thin bright stripes at panel borders
    float seamZ   = smoothstep(0.04, 0.0, min(zLocal, 1.0 - zLocal));
    float seamA   = smoothstep(0.04, 0.0, min(aLocal, 1.0 - aLocal));
    float seam    = max(seamZ, seamA);

    // Per-panel concrete tone — small grayscale variation
    float t        = hash21(vec2(aCell, zCell));
    vec3  concrete = vec3(0.42 + 0.15 * t, 0.42 + 0.14 * t, 0.44 + 0.15 * t);

    // Subtle aggregate speckle (poured concrete texture feel)
    float speckle = hash21(vec2(aCell * 17.0 + zLocal * 31.0, zCell * 13.0 + aLocal * 19.0));
    concrete     *= 0.85 + 0.30 * (speckle - 0.5);

    // Recession: near panels dim, far panels bright
    float rec     = recession(r);
    vec3  col     = concrete * mix(0.06, 1.1, rec);

    // Bright concrete seams accentuate the perspective
    col          += vec3(1.0, 0.95, 0.85) * seam * mix(0.15, 1.4, rec);

    // Cool ambient lamp at the vanishing end (peaks above 1.6)
    col          += vec3(0.55, 0.75, 1.05) * pow(rec, 5.0) * (0.9 + 0.5 * audio);
    return col;
}

// ─── main ─────────────────────────────────────────────────────────────
void main() {
    // Aspect-corrected centered coords
    vec2 uv  = isf_FragNormCoord.xy * 2.0 - 1.0;
    uv.x    *= RENDERSIZE.x / RENDERSIZE.y;

    float t      = TIME;
    float audio  = clamp(audioReact, 0.0, 2.0);
    float bass   = clamp(audioBass, 0.0, 1.0) * audio;
    float mid    = clamp(audioMid,  0.0, 1.0) * audio;
    float treble = clamp(audioHigh, 0.0, 1.0) * audio;

    // Mid-band drives lateral "handheld" camera sway
    vec2 sway = vec2(
        sin(t * 0.7) * 0.025 + sin(t * 1.9) * 0.012,
        cos(t * 0.5) * 0.018 + cos(t * 2.1) * 0.010
    ) * (0.4 + 1.2 * mid);

    // Off-center vanishing point — held away from dead-center
    vec2 vp   = vec2(vanishOffX, vanishOffY) + sway;
    vec2 d    = uv - vp;

    // Radial "zoom breathing" — the whole tunnel inhales with the low end.
    // LINEAR follower (no knees): beatless ambient swells shift every pixel
    // radially in proportion to the level. Silence multiplies by exactly 1.0.
    d        *= 1.0 - (0.10 * audioBass + 0.06 * audioMid) * min(audio, 1.0);

    // Optional twist around the vanishing point (treble sparkle modulation)
    float twAng = tunnelTwist * (0.6 + 0.4 * sin(t * 0.27)) + treble * 0.15;
    float c = cos(twAng), s = sin(twAng);
    d        = mat2(c, -s, s, c) * d;

    // Polar mapping: r = distance from vanishing point, a = angle
    float r     = length(d);
    float angle = atan(d.y, d.x);

    // Log-distance "depth": grows toward the vanishing point.
    // log(1/r) is large at center, small (or negative) at edges.
    float depth = -log(max(r, 0.012));

    // Forward flight — constant rate (silence-identical) + integrated band
    // time. The old "+ 0.6 * bass" push made frame motion track the bass
    // DERIVATIVE (90° out of phase — the shift-null test claims that phase and
    // ambient scored 0). audioBassTime/audioMidTime integrate the smoothed
    // levels, so flight SPEED tracks the level itself: per-frame change is
    // linear in the band mix, in phase, at lag 0. Frozen in silence.
    float flightZ = t * (tunnelSpeed * 0.6 + 0.22)
                  + (0.90 * audioBassTime + 0.50 * audioMidTime) * min(audio, 1.0);

    // Choose mood
    int   m   = int(mood + 0.5);
    vec3  col = vec3(0.0);
    if      (m == 0) col = moodStargate (depth, angle, r, flightZ, audio, treble);
    else if (m == 1) col = moodMcCall   (depth, angle, r, flightZ, audio);
    else if (m == 2) col = moodEscher   (depth, angle, r, flightZ, audio);
    else if (m == 3) col = moodBauhaus  (depth, angle, r, flightZ, audio);
    else             col = moodBrutalist(depth, angle, r, flightZ, audio);

    // ── Atmosphere / haze: lifts the deeper parts of the frame ────────
    float haze   = exp(-r * 1.3) * hazeDensity;
    vec3  hazeC;
    if      (m == 0) hazeC = vec3(0.30, 0.14, 0.45); // stargate violet
    else if (m == 1) hazeC = vec3(0.55, 0.62, 0.80); // McCall white-blue
    else if (m == 2) hazeC = vec3(0.34, 0.24, 0.14); // Escher umber
    else if (m == 3) hazeC = vec3(1.00, 0.96, 0.88); // Bauhaus paper
    else             hazeC = vec3(0.20, 0.26, 0.36); // brutalist cool grey
    col         += hazeC * haze * 0.65;

    // ── Vanishing-point hot spot (treble sparkle) ─────────────────────
    float hotR   = 0.07 + 0.03 * sin(t * 2.3);
    float hot    = exp(-pow(r / hotR, 2.0));
    vec3  hotC;
    if      (m == 0) hotC = vec3(1.7, 1.4, 2.1);
    else if (m == 1) hotC = vec3(1.7, 1.65, 1.55);
    else if (m == 2) hotC = vec3(1.7, 1.4, 0.90);
    else if (m == 3) hotC = vec3(1.7, 1.5, 1.00);
    else             hotC = vec3(1.0, 1.2, 1.6);
    col         += hotC * hot * (0.8 + 1.6 * treble);

    // Tiny multi-octave sparkle near the vanishing point on treble
    float spk    = 0.0;
    for (int i = 0; i < 6; i++) {
        float fi = float(i);
        vec2  sp = vp + vec2(0.04 * cos(t * (1.7 + fi)),
                             0.04 * sin(t * (1.3 + fi * 0.7)));
        spk     += exp(-length(uv - sp) * 60.0);
    }
    col         += vec3(1.4, 1.2, 1.6) * spk * (0.15 + treble) * 0.8;

    // ── Soft vignette pull (corners) ──────────────────────────────────
    float vig    = 1.0 - 0.28 * dot(uv * 0.55, uv * 0.55);
    col         *= vig;

    // Linear whole-frame follower — keeps band correlation now that the
    // flight speed no longer teleports with bass (bands are pre-smoothed).
    col         *= 1.0 + (0.25 * audioBass + 0.15 * audioMid) * min(audio, 1.0);

    // Final exposure (LINEAR HDR — no tonemap; host applies ACES)
    col         *= exposure;

    // ---- universal color block (defaults = no-op) ----
    float ucL = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(ucL), col, colorBoost);
    if (hueShift > 0.0005) {
        float hA = hueShift * 6.2831853;
        float hC = cos(hA), hS = sin(hA);
        mat3 hM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
                + hC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
                + hS * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
        col = clamp(hM * col, 0.0, 1.0);
    }
    col = mix(col, bgColor.rgb, bgColor.a * (1.0 - smoothstep(0.0, 0.35, ucL)));
    gl_FragColor = vec4(col, 1.0);
}
