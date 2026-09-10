/*{
  "DESCRIPTION": "Data Minimal Lines — a typographic data score on warm paper. Three quiet agents lay marks across the page: thin technical hairlines, asemic clusters of dots/brackets/symbols, and sparse solid black bars. The live cue lays in as the central spoken line, typewriter-revealed. Generous whitespace, gallery restraint — at silence the page is almost still; with voice and energy, marks drift, bars elongate, hairlines tilt. Three player channels, three mark types. Abstract — never a chart, never bars-as-EQ.",
  "CREDIT": "easel a-list — data_minimal_lines_text",
  "CATEGORIES": [
    "Generator",
    "Text",
    "A-List"
  ],
  "INPUTS": [
    {
      "NAME": "energyA",
      "LABEL": "Lines Agent",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.35,
      "BIND": "player[1].energy"
    },
    {
      "NAME": "energyB",
      "LABEL": "Glyph Agent",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.35,
      "BIND": "player[2].energy"
    },
    {
      "NAME": "energyC",
      "LABEL": "Bars Agent",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.35,
      "BIND": "player[3].energy"
    },
    {
      "NAME": "aliveA",
      "LABEL": "Lines Active",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 1,
      "BIND": "player[1].active"
    },
    {
      "NAME": "aliveB",
      "LABEL": "Glyph Active",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 1,
      "BIND": "player[2].active"
    },
    {
      "NAME": "aliveC",
      "LABEL": "Bars Active",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 1,
      "BIND": "player[3].active"
    },
    {
      "NAME": "grain",
      "LABEL": "Paper Grain",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1.5,
      "DEFAULT": 0.55
    },
    {
      "NAME": "fidBloom",
      "LABEL": "Glow",
      "TYPE": "float",
      "DEFAULT": 0.55,
      "MIN": 0,
      "MAX": 1.5
    },
    {
      "NAME": "fidDither",
      "LABEL": "Dither",
      "TYPE": "float",
      "DEFAULT": 0.85,
      "MIN": 0,
      "MAX": 1
    },
    {
      "NAME": "fidGamma",
      "LABEL": "Gamma",
      "TYPE": "float",
      "DEFAULT": 0.6,
      "MIN": 0,
      "MAX": 1
    },
    {
      "NAME": "fidEdgeGlow",
      "LABEL": "Edge Glow",
      "TYPE": "float",
      "DEFAULT": 0.55,
      "MIN": 0,
      "MAX": 2
    },
    {
      "NAME": "fidVignette",
      "LABEL": "Vignette",
      "TYPE": "float",
      "DEFAULT": 0.45,
      "MIN": 0,
      "MAX": 1.5
    },
    {
      "NAME": "fidGrain",
      "LABEL": "Grain",
      "TYPE": "float",
      "DEFAULT": 0.35,
      "MIN": 0,
      "MAX": 1
    },
    {
      "NAME": "lineDensity",
      "LABEL": "Line Density",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.55,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "shapeCount",
      "LABEL": "Shape Count",
      "TYPE": "long",
      "DEFAULT": 6,
      "VALUES": [
        3,
        4,
        6,
        8,
        10,
        12
      ],
      "LABELS": [
        "3",
        "4",
        "6",
        "8",
        "10",
        "12"
      ],
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "motionSpeed",
      "LABEL": "Motion Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 0.7,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "motionDrift",
      "LABEL": "Drift Speed",
      "TYPE": "float",
      "DEFAULT": 1.3,
      "MIN": 0,
      "MAX": 3,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "motionJitter",
      "LABEL": "Jitter",
      "TYPE": "float",
      "DEFAULT": 0.25,
      "MIN": 0,
      "MAX": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "motionFlicker",
      "LABEL": "Flicker",
      "TYPE": "float",
      "DEFAULT": 0.15,
      "MIN": 0,
      "MAX": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "motionSway",
      "LABEL": "Sway",
      "TYPE": "float",
      "DEFAULT": 0.5,
      "MIN": 0,
      "MAX": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "motionChaos",
      "LABEL": "Chaos",
      "TYPE": "float",
      "DEFAULT": 0.45,
      "MIN": 0,
      "MAX": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "palette",
      "LABEL": "Palette",
      "TYPE": "long",
      "DEFAULT": 0,
      "VALUES": [
        0,
        1,
        2,
        3
      ],
      "LABELS": [
        "Ivory Score",
        "Slate",
        "Vellum",
        "Inverse"
      ],
      "GROUP": "Color"
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
      "NAME": "msg",
      "LABEL": "Spoken Line",
      "TYPE": "text",
      "DEFAULT": "what we require is silence",
      "MAX_LENGTH": 48,
      "BIND": "cue.latest",
      "GROUP": "Text"
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
      "NAME": "drift",
      "LABEL": "Score Drift",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 0.7,
      "BIND": "audio.level",
      "GROUP": "Audio Reactivity"
    },
    {
      "NAME": "audioDepth",
      "LABEL": "Audio Depth",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 0.9,
      "GROUP": "Audio Reactivity"
    }
  ]
}*/

// ── FIDELITY KIT v2 (text-safe cinematic polish) ────────────────────
// Pushes the visual elements toward fluid-sim fidelity WITHOUT touching
// glyph shapes (no displacement, no chromatic shift — every stage is
// pure color math on the final RGB). Stages:
//   1. EDGE GLOW — dFdx/dFdy on luminance detects bright edges and
//      adds a per-pixel halo. Captures glow at text edges too, making
//      letters read as lit rather than printed.
//   2. HEADROOM BLOOM — bright pixels self-lift (HDR-ish look) with a
//      wider mid-pixel kick than v1's highlights-only version.
//   3. VIGNETTE — radial darken, anchors the eye to center, adds
//      focal depth.
//   4. ANIMATED GRAIN — sub-percept noise broken in time, kills the
//      flat-vector feel without showing as visible texture.
//   5. SOFT REINHARD TONEMAP — keeps overshoot from hard-clipping.
//   6. BLUE-NOISE DITHER + GAMMA — same as v1, ends the chain.
vec3 fidApply(vec3 col, vec2 frag) {
    float l = dot(col, vec3(0.299, 0.587, 0.114));

    // 1. Edge glow — gradient magnitude of luminance.
    vec2  lg   = vec2(dFdx(l), dFdy(l));
    float edge = clamp(length(lg) * 7.0, 0.0, 1.0);
    col += col * edge * fidEdgeGlow * 1.50;

    // 2. Headroom bloom (widened from v1).
    float headroom = smoothstep(0.28, 0.95, l);
    col += col * headroom * fidBloom * 1.80;

    // 3. Vignette — quadratic radial darken.
    vec2  uvN = frag / RENDERSIZE - 0.5;
    float vig = 1.0 - dot(uvN, uvN) * 1.80 * fidVignette;
    col *= clamp(vig, 0.0, 1.0);

    // 4. Animated grain — phase by TIME so it scintillates instead of
    //    sitting still (a static dither, which we still add at stage 6
    //    for banding control). Magnitude small enough to read as
    //    "atmosphere" not "noisy."
    float g = fract(sin(dot(frag + vec2(TIME * 73.0, TIME * 41.0),
                            vec2(12.9898, 78.233))) * 43758.5453);
    col += (g - 0.5) * fidGrain * 0.045;

    // 5. Soft Reinhard tonemap.
    col = col / (1.0 + col * 0.18);

    // 6. Static blue-noise dither + linear→sRGB.
    float n = fract(sin(dot(frag, vec2(12.9898, 78.233))) * 43758.5453);
    col += (n - 0.5) * (1.0 / 255.0) * fidDither;
    col = mix(col, pow(max(col, 0.0), vec3(1.0 / 2.2)), fidGamma);
    return col;
}


// ─── MOTION KIT (shared across recent text shaders) ─────────────────
// Adds life on top of each shader's native animation: a breathing sway +
// continuous drift + coarse reseeding jitter on the working coord, plus a
// brightness flicker. Driven by the motion* uniforms. mkMotion() returns an
// offset to add to a coord; mkFlicker() a brightness multiplier.
float mkHash(vec2 p){ p = fract(p * vec2(127.1, 311.7)); p += dot(p, p + 34.5); return fract(p.x * p.y); }
vec2 mkMotion(vec2 q, float t){
    float ch = 0.4 + motionChaos;
    vec2 sway  = vec2(sin(t * 0.32 + q.y * 1.8), cos(t * 0.27 + q.x * 1.6)) * motionSway  * 0.09;
    vec2 drift = vec2(sin(t * 0.12 * ch), cos(t * 0.10 * ch))            * motionDrift * 0.05;
    // Smooth organic wander — layered incommensurate sines, slow + no stepping.
    float f = 1.0 + 1.2 * motionChaos;
    vec2 jit = vec2(
        sin(t * 0.70 * f + q.y * 3.1) * 0.6 + sin(t * 0.45 * f + q.x * 2.3 + 1.7) * 0.4,
        cos(t * 0.60 * f + q.x * 2.7) * 0.6 + cos(t * 0.50 * f + q.y * 2.9 + 4.2) * 0.4
    ) * motionJitter * 0.05;
    return sway + drift + jit;
}
float mkFlicker(vec2 q, float t){
    // Smooth, slow brightness undulation + soft scanline (no hard strobe).
    float n    = 0.5 + 0.5 * sin(t * 2.0 + q.x * 7.0 + q.y * 5.0);
    float scan = 0.5 + 0.5 * sin(q.y * 180.0 + t * 3.0);
    return 1.0 - motionFlicker * (0.5 * n + 0.30 * scan);
}


// ════════════════════════════════════════════════════════════════════════
//  data_minimal_lines_text — a quiet typographic data score.
//
//  Three agents share a sparse page:
//    Agent A (Lines)  — thin technical hairlines, depth-staggered.
//    Agent B (Glyphs) — clusters of asemic marks (dots, brackets, ticks).
//    Agent C (Bars)   — sparse solid black bars (the "data" elements).
//  Each agent's count + opacity scales with its player channel; mute one
//  and that mark-type visibly thins. Cue text lays in as the spoken line.
//  Restraint is the rule. fwidth-AA throughout, generous whitespace.
// ════════════════════════════════════════════════════════════════════════

#define SPACE_CH  26
#define MAX_LINES 12
#define MAX_GLYPH 14
#define MAX_BARS  12

// ─── ISF font atlas (Easel contract) ─────────────────────────────────
float sampleChar(int ch, vec2 uv) {
    if (ch < 0 || ch > 36) return 0.0;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    return texture2D(fontAtlasTex, vec2((float(ch) + uv.x) / 37.0, uv.y)).r;
}

int getChar(int slot) {
    if (slot ==  0) return int(msg_0);
    if (slot ==  1) return int(msg_1);
    if (slot ==  2) return int(msg_2);
    if (slot ==  3) return int(msg_3);
    if (slot ==  4) return int(msg_4);
    if (slot ==  5) return int(msg_5);
    if (slot ==  6) return int(msg_6);
    if (slot ==  7) return int(msg_7);
    if (slot ==  8) return int(msg_8);
    if (slot ==  9) return int(msg_9);
    if (slot == 10) return int(msg_10);
    if (slot == 11) return int(msg_11);
    if (slot == 12) return int(msg_12);
    if (slot == 13) return int(msg_13);
    if (slot == 14) return int(msg_14);
    if (slot == 15) return int(msg_15);
    if (slot == 16) return int(msg_16);
    if (slot == 17) return int(msg_17);
    if (slot == 18) return int(msg_18);
    if (slot == 19) return int(msg_19);
    if (slot == 20) return int(msg_20);
    if (slot == 21) return int(msg_21);
    if (slot == 22) return int(msg_22);
    if (slot == 23) return int(msg_23);
    if (slot == 24) return int(msg_24);
    if (slot == 25) return int(msg_25);
    if (slot == 26) return int(msg_26);
    if (slot == 27) return int(msg_27);
    if (slot == 28) return int(msg_28);
    if (slot == 29) return int(msg_29);
    if (slot == 30) return int(msg_30);
    if (slot == 31) return int(msg_31);
    if (slot == 32) return int(msg_32);
    if (slot == 33) return int(msg_33);
    if (slot == 34) return int(msg_34);
    if (slot == 35) return int(msg_35);
    if (slot == 36) return int(msg_36);
    if (slot == 37) return int(msg_37);
    if (slot == 38) return int(msg_38);
    if (slot == 39) return int(msg_39);
    if (slot == 40) return int(msg_40);
    if (slot == 41) return int(msg_41);
    if (slot == 42) return int(msg_42);
    if (slot == 43) return int(msg_43);
    if (slot == 44) return int(msg_44);
    if (slot == 45) return int(msg_45);
    if (slot == 46) return int(msg_46);
    if (slot == 47) return int(msg_47);
    return -1;
}

int charCount() {
    int n = int(msg_len);
    if (n <= 0) return 0;
    if (n > 48) return 48;
    return n;
}

// ─── Hash / noise ────────────────────────────────────────────────────
float h11(float n) { return fract(sin(n * 127.1) * 43758.5453); }
float h12(vec2 p)  { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2  h21(float n) { return vec2(h11(n), h11(n + 17.31)); }

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = h12(i);
    float b = h12(i + vec2(1.0, 0.0));
    float c = h12(i + vec2(0.0, 1.0));
    float d = h12(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
float fbm2(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 3; i++) {
        v += a * vnoise(p);
        p = p * 2.07 + vec2(11.3, 5.7);
        a *= 0.55;
    }
    return v;
}

// ─── SDF: thin line segment (capsule) ────────────────────────────────
float sdSegment(vec2 p, vec2 a, vec2 b, float r) {
    vec2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

// ─── SDF: rotated rectangle (the black "data bars") ──────────────────
float sdRotRect(vec2 p, vec2 c, vec2 half_, float ang) {
    float ca = cos(ang), sa = sin(ang);
    vec2 q = p - c;
    q = vec2(ca * q.x + sa * q.y, -sa * q.x + ca * q.y);
    vec2 d = abs(q) - half_;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

// ─── Asemic micro-glyph — picks a tiny SDF mark by seed ──────────────
// 0:dot 1:tick 2:bracket-L 3:bracket-R 4:delta 5:dash 6:double-dot
float asemicMark(vec2 p, vec2 c, float size, int kind) {
    vec2 q = (p - c) / size;
    float d = 1e6;
    if (kind == 0) {
        d = length(q) - 0.18;
    } else if (kind == 1) {
        d = sdSegment(q, vec2(0.0, -0.35), vec2(0.0, 0.35), 0.06);
    } else if (kind == 2) {
        float a = sdSegment(q, vec2(-0.25, -0.4), vec2(-0.25, 0.4), 0.05);
        float b = sdSegment(q, vec2(-0.25,  0.4), vec2(-0.05, 0.4), 0.05);
        float e = sdSegment(q, vec2(-0.25, -0.4), vec2(-0.05,-0.4), 0.05);
        d = min(min(a, b), e);
    } else if (kind == 3) {
        float a = sdSegment(q, vec2( 0.25, -0.4), vec2( 0.25, 0.4), 0.05);
        float b = sdSegment(q, vec2( 0.25,  0.4), vec2( 0.05, 0.4), 0.05);
        float e = sdSegment(q, vec2( 0.25, -0.4), vec2( 0.05,-0.4), 0.05);
        d = min(min(a, b), e);
    } else if (kind == 4) {
        float a = sdSegment(q, vec2(-0.32, -0.3), vec2( 0.32, -0.3), 0.05);
        float b = sdSegment(q, vec2(-0.32, -0.3), vec2( 0.00,  0.32), 0.05);
        float e = sdSegment(q, vec2( 0.32, -0.3), vec2( 0.00,  0.32), 0.05);
        d = min(min(a, b), e);
    } else if (kind == 5) {
        d = sdSegment(q, vec2(-0.4, 0.0), vec2(0.4, 0.0), 0.05);
    } else {
        float a = length(q - vec2(-0.18, 0.0)) - 0.10;
        float b = length(q - vec2( 0.18, 0.0)) - 0.10;
        d = min(a, b);
    }
    return d;
}

// ─── Palette ────────────────────────────────────────────────────────
struct Pal { vec3 paper; vec3 ink; vec3 ink2; vec3 accent; };
Pal getPalette(int idx) {
    Pal P;
    if (idx == 0) {           // Ivory Score
        P.paper  = vec3(0.955, 0.945, 0.918);
        P.ink    = vec3(0.07, 0.06, 0.06);
        P.ink2   = vec3(0.30, 0.28, 0.27);
        P.accent = vec3(0.55, 0.20, 0.18);
    } else if (idx == 1) {    // Slate
        P.paper  = vec3(0.86, 0.86, 0.88);
        P.ink    = vec3(0.10, 0.10, 0.13);
        P.ink2   = vec3(0.34, 0.34, 0.38);
        P.accent = vec3(0.18, 0.32, 0.55);
    } else if (idx == 2) {    // Vellum
        P.paper  = vec3(0.97, 0.94, 0.86);
        P.ink    = vec3(0.10, 0.07, 0.04);
        P.ink2   = vec3(0.36, 0.30, 0.20);
        P.accent = vec3(0.50, 0.30, 0.10);
    } else {                  // Inverse (ink ground)
        P.paper  = vec3(0.06, 0.05, 0.06);
        P.ink    = vec3(0.94, 0.93, 0.90);
        P.ink2   = vec3(0.62, 0.61, 0.58);
        P.accent = vec3(0.95, 0.65, 0.30);
    }
    return P;
}

// Composite an ink stroke onto the page using fwidth-AA.
void inkOver(inout vec3 col, float sdf, vec3 ink, float opacity) {
    float aa = max(fwidth(sdf), 0.0008);
    float a  = (1.0 - smoothstep(-aa, aa, sdf)) * opacity;
    col = mix(col, ink, a);
}

void main() {
    vec2 res    = RENDERSIZE;
    vec2 uv     = gl_FragCoord.xy / res;
    uv += mkMotion(uv, TIME);
    float aspect = res.x / res.y;
    // Aspect-corrected normalized coords, y up, page width = aspect.
    vec2 p; p.x = (uv.x - 0.5) * aspect; p.y = uv.y - 0.5;

    float t      = TIME * max(motionSpeed, 0.0);
    float audio  = clamp(audioDepth, 0.0, 2.0);
    float lvl    = clamp(drift, 0.0, 2.0);          // bound to audio.level
    float bass   = audioBass;
    float treb   = audioHigh;

    float eA = clamp(energyA, 0.0, 1.0) * clamp(aliveA, 0.0, 1.0);
    float eB = clamp(energyB, 0.0, 1.0) * clamp(aliveB, 0.0, 1.0);
    float eC = clamp(energyC, 0.0, 1.0) * clamp(aliveC, 0.0, 1.0);

    Pal P = getPalette(int(palette));
    // universal background: the paper IS this shader's background region
    P.paper = mix(P.paper, bgColor.rgb, bgColor.a);

    // ─── Paper ground with subtle marbling — keeps the page alive ──
    float wob = fbm2(p * 1.4 + vec2(t * 0.04, 0.0));
    vec3 col  = P.paper * (1.0 - 0.05 * (wob - 0.5));
    // Soft horizontal stain so the page reads as material, not screen.
    float stain = exp(-pow(p.y + 0.05, 2.0) * 1.8) * 0.04;
    col = mix(col, mix(P.paper, P.ink2, 0.1), stain);

    // Edge vignette — the page sits in the canvas like a sheet.
    float vig = smoothstep(0.55, 0.20, length(uv - 0.5));
    col *= 0.95 + 0.07 * vig;

    // ════════════════════════════════════════════════════════════════
    //  AGENT A — thin technical hairlines  (player[1])
    //  Quiet at low energy: maybe 2 visible. Builds to a network.
    // ════════════════════════════════════════════════════════════════
    int lineN = int(floor(mix(2.0, float(MAX_LINES), clamp(lineDensity, 0.0, 1.0))));
    // Visible count modulated by energyA — silence ≈ 1 line, full ≈ all.
    float visLines = mix(1.0, float(lineN), 0.35 + 0.65 * eA);
    for (int i = 0; i < MAX_LINES; i++) {
        if (i >= lineN) break;
        float fi = float(i);
        // Per-line seed.
        vec2  s1 = h21(fi * 3.13 + 0.7);
        vec2  s2 = h21(fi * 5.71 + 1.3);
        // Page positions, biased toward horizontal score lines.
        vec2 a, b;
        float kind = h11(fi * 9.71);
        if (kind < 0.55) {
            // mostly horizontal sweep — keep a wide span, slight tilt
            float y = (s1.y - 0.5) * 0.85;
            float x0 = -aspect * 0.5 * (0.35 + 0.55 * s2.x);
            float x1 =  aspect * 0.5 * (0.35 + 0.55 * s2.y);
            float tilt = (s2.x - 0.5) * 0.08 * (0.5 + lvl);
            a = vec2(x0, y - tilt);
            b = vec2(x1, y + tilt);
        } else if (kind < 0.85) {
            // diagonal long line
            float ang = (s1.x - 0.5) * 0.9;
            vec2  c   = vec2((s2.x - 0.5) * aspect * 0.8, (s1.y - 0.5) * 0.7);
            float L   = 0.35 + 0.55 * s2.y;
            a = c + vec2(cos(ang), sin(ang)) * L;
            b = c - vec2(cos(ang), sin(ang)) * L;
        } else {
            // short vertical tick
            float x = (s1.x - 0.5) * aspect * 0.9;
            float y = (s2.y - 0.5) * 0.8;
            float L = 0.04 + 0.06 * s1.y;
            a = vec2(x, y - L); b = vec2(x, y + L);
        }
        // Slow drift on energy + audio.
        vec2 d = vec2(sin(t * 0.13 + fi * 1.7), cos(t * 0.11 + fi * 2.3));
        a += d * 0.012 * (0.4 + lvl + 0.6 * eA);
        b -= d * 0.012 * (0.4 + lvl + 0.6 * eA);

        // Hairline radius — anti-alias with fwidth-AA. Bass adds a hair.
        float r = 0.0010 + 0.0009 * h11(fi * 7.7) + 0.0006 * bass * audio * eA;
        float d2 = sdSegment(p, a, b, r);

        // Fade-in based on whether this line is "alive" at current visLines.
        float alive = smoothstep(fi + 0.0, fi + 0.6, visLines);
        vec3 ink = (mod(fi, 7.0) == 3.0) ? mix(P.ink, P.accent, 0.35) : P.ink;
        inkOver(col, d2, ink, 0.85 * alive * (0.55 + 0.55 * (0.5 + eA * 0.5)));
    }

    // ════════════════════════════════════════════════════════════════
    //  AGENT B — asemic glyph clusters  (player[2])
    //  Tight scatters of small marks. Quiet by default.
    // ════════════════════════════════════════════════════════════════
    for (int i = 0; i < MAX_GLYPH; i++) {
        float fi = float(i);
        // Per-glyph seed.
        vec2 sB = h21(fi * 11.13 + 5.0);
        vec2 sC = h21(fi * 4.77 + 19.0);
        int  kind = int(floor(h11(fi * 2.31) * 6.999));

        // Each cluster sits in a slow-drifting position.
        vec2 base = vec2((sB.x - 0.5) * aspect * 0.92, (sB.y - 0.5) * 0.88);
        base.x += 0.018 * sin(t * 0.21 + fi * 1.31) * (0.5 + lvl);
        base.y += 0.014 * cos(t * 0.17 + fi * 2.07) * (0.5 + lvl);

        // Cluster has 3 micro-marks in a row (asemic word).
        float clusterFade = smoothstep(0.10, 0.70, eB) + 0.18;
        clusterFade *= step(h11(fi * 0.93), 0.55 + 0.45 * eB);
        if (clusterFade < 0.02) continue;

        float spacing = 0.022 + 0.012 * sC.x;
        float size    = 0.013 + 0.005 * sC.y + 0.004 * treb * audio * eB;

        for (int j = 0; j < 3; j++) {
            float fj = float(j);
            vec2  off = vec2(spacing * (fj - 1.0), 0.0);
            int   k  = int(mod(float(kind + j), 7.0));
            float d  = asemicMark(p, base + off, size, k);
            vec3 ink = (h11(fi * 0.71 + fj) < 0.15) ? P.accent : P.ink;
            inkOver(col, d, ink, 0.88 * clusterFade);
        }

        // Occasionally pair the cluster with a tiny adjacent dotted dash.
        if (h11(fi * 1.91) > 0.66) {
            vec2 dotBase = base + vec2(spacing * 2.4, 0.0);
            for (int k = 0; k < 5; k++) {
                vec2 dc = dotBase + vec2(float(k) * 0.010, 0.0);
                float dd = length(p - dc) - 0.0018;
                inkOver(col, dd, P.ink, 0.85 * clusterFade);
            }
        }
    }

    // ════════════════════════════════════════════════════════════════
    //  AGENT C — sparse black data bars  (player[3])
    //  Real depth: 2 layers of bars at different z; near drifts more.
    // ════════════════════════════════════════════════════════════════
    int barN = int(shapeCount);
    if (barN > MAX_BARS) barN = MAX_BARS;
    float visBars = mix(1.0, float(barN), 0.30 + 0.70 * eC);
    for (int i = 0; i < MAX_BARS; i++) {
        if (i >= barN) break;
        float fi = float(i);
        vec2 sA = h21(fi * 23.71 + 9.0);
        vec2 sB = h21(fi * 7.13 + 33.0);

        // Layer: 0=far (small, faint), 1=near (larger, opaque).
        float layer = step(0.6, h11(fi * 0.41));
        float z     = mix(0.55, 1.0, layer);

        // Position. Far layer scrolls slowly; near drifts faster — parallax.
        vec2 c;
        c.x = (sA.x - 0.5) * aspect * 0.92;
        c.y = (sA.y - 0.5) * 0.88;
        float driftPx = 0.018 * (0.4 + lvl + 0.5 * eC);
        c.x += sin(t * 0.10 + fi * 0.7) * driftPx * (0.5 + 0.7 * layer);
        c.y += cos(t * 0.08 + fi * 1.3) * driftPx * (0.5 + 0.7 * layer);

        // Size — short flat bar. Length swells slightly with bass on near layer.
        float lengthMul = 1.0 + 0.18 * bass * audio * layer * eC;
        vec2 half_;
        half_.x = (0.030 + 0.075 * sB.x) * lengthMul * z;
        half_.y = (0.008 + 0.006 * sB.y) * z;
        float ang = (sB.x - 0.5) * 0.15;     // mostly horizontal

        float d = sdRotRect(p, c, half_, ang);
        float alive = smoothstep(fi + 0.0, fi + 0.6, visBars);
        // Far layer is faint grey; near layer is full black. Real depth feel.
        vec3 ink = mix(P.ink2, P.ink, layer);
        float op = mix(0.55, 0.95, layer) * alive;
        inkOver(col, d, ink, op);
    }

    // ════════════════════════════════════════════════════════════════
    //  CUE TEXT — the spoken line. Centered, typewriter via msgAge.
    //  Crisp, technical. Underline beneath. Caret at the write head.
    // ════════════════════════════════════════════════════════════════
    int total = charCount();
    if (total > 0) {
        bool live = msgAge >= 0.0;
        const float CPS = 28.0;
        float revealed = live ? min(float(total), msgAge * CPS) : float(total);
        int   visN = int(floor(revealed));
        float caretFrac = revealed - float(visN);

        // Layout: middle-ish, slight offset right to mimic the score.
        float capH = 0.030;
        float capW = capH * 0.62;
        float strW = float(total) * capW * 1.06;
        float maxW = aspect * 0.78;
        if (strW > maxW) {
            float k = maxW / strW;
            capH *= k; capW *= k; strW *= k;
        }
        float capY = 0.02;
        float capX0 = -strW * 0.45;   // anchor slightly left-of-center

        // Thin underline that grows with reveal.
        float revFrac = (total > 0) ? (revealed / float(total)) : 0.0;
        float underY  = capY - capH * 0.22;
        float underX1 = capX0 + strW * revFrac;
        float underSDF = sdSegment(p, vec2(capX0, underY), vec2(underX1, underY), 0.0012);
        inkOver(col, underSDF, P.ink, 0.85);

        // Glyph walk.
        for (int i = 0; i < 48; i++) {
            if (i >= visN)  break;
            if (i >= total) break;
            int ch = getChar(i);
            float cellX0 = capX0 + float(i) * capW * 1.06;
            float cellX1 = cellX0 + capW;
            if (p.x < cellX0 - capW || p.x > cellX1 + capW) continue;
            if (p.y < capY - capH * 0.10 || p.y > capY + capH * 1.10) continue;
            if (ch < 0 || ch > 35) continue;
            vec2 gUV;
            gUV.x = clamp((p.x - cellX0) / capW, 0.0, 1.0);
            // p.y is y-UP world; capY anchors the BOTTOM of the caption
            // cell. The host font atlas stores letter-top at v=1, so a
            // direct y-up→v mapping puts letter-top at screen-top. The
            // previous `1.0 -` here flipped glyphs upside down.
            gUV.y = clamp((p.y - capY) / capH, 0.0, 1.0);
            float s  = sampleChar(ch, gUV);
            float aa = max(fwidth(s), 0.001);
            float a  = smoothstep(0.45 - aa, 0.45 + aa, s);
            if (a < 0.001) continue;
            col = mix(col, P.ink, a);
        }

        // Caret at the write head — thin vertical hairline, blink modulated.
        float caretX = capX0 + float(visN) * capW * 1.06;
        vec2  ca = vec2(caretX, capY + capH * 0.05);
        vec2  cb = vec2(caretX, capY + capH * 0.95);
        float caretSDF = sdSegment(p, ca, cb, 0.0012);
        float blink = step(0.5, fract(TIME * 1.6));
        float typing = smoothstep(0.05, 0.9, caretFrac);
        float caretA = live ? mix(0.9, blink, typing) : blink;
        inkOver(col, caretSDF, P.ink, 0.85 * caretA);
    }

    // ════════════════════════════════════════════════════════════════
    //  Paper grain — fibre, never a pixel grid.
    // ════════════════════════════════════════════════════════════════
    float g = fbm2(p * 60.0) - 0.5;
    col += g * 0.022 * clamp(grain, 0.0, 1.5);

    // Slow gallery sheen — barely perceptible, intentional motion floor.
    float sheen = smoothstep(0.0, 0.5, sin((p.x - p.y * 0.5) * 1.6 - t * 0.18) * 0.5 + 0.5);
    col += pow(sheen, 4.0) * 0.020 * (P.paper - 0.5);

    // Audio-bound subtle pulse of contrast — never garish. `bass`/`treb`
    // are the live engine bands (audioBass/audioHigh); `lvl`/`audio` are
    // the host-BIND'd score-drift knobs. Both feed the same soft pulse so
    // the page visibly breathes with the mix, not just the BIND path.
    float bassHit = clamp(bass + treb, 0.0, 2.0);
    float pulse = 1.0 + 0.04 * lvl * audio + 0.28 * bassHit;
    col = (col - P.paper) * pulse + P.paper;
    // Whole-page breathing: a faint ink-ward lift so the paper itself (not
    // just the marks) visibly answers the mix — musical, not garish.
    col += (P.ink - P.paper) * 0.020 * bassHit;

    // ---- universal color block (defaults = no-op) ----
    vec3 uc = fidApply(clamp(col, 0.0, 1.0), gl_FragCoord.xy);
    float ucL = dot(uc, vec3(0.299, 0.587, 0.114));
    uc = mix(vec3(ucL), uc, colorBoost);                   // saturation
    if (hueShift > 0.0005) {                               // cheap hue rotate (YIQ)
        float hA = hueShift * 6.2831853;
        float hC = cos(hA), hS = sin(hA);
        mat3 hM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
                + hC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
                + hS * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
        uc = clamp(hM * uc, 0.0, 1.0);
    }

    gl_FragColor = vec4(uc, 1.0);
}
