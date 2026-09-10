/*{
  "CATEGORIES": ["Generator", "Audio Reactive", "Minimal"],
  "DESCRIPTION": "Bauhaus Tiles — monochrome geometric grid. Each cell holds one primitive (disc, half-moon, quarter arc, hollow square, diagonal) in black or white; every cell listens to its own slice of the spectrum and inverts when its band speaks, beats rotate a scattered subset ninety degrees with an eased snap. Strict grayscale geometry.",
  "INPUTS": [
    {"NAME": "gridSize",   "LABEL": "Grid Size",     "TYPE": "float", "MIN": 3.0, "MAX": 14.0, "DEFAULT": 7.0},
    {"NAME": "margin",     "LABEL": "Cell Margin",   "TYPE": "float", "MIN": 0.0, "MAX": 0.35, "DEFAULT": 0.10},
    {"NAME": "lineWeight", "LABEL": "Line Weight",   "TYPE": "float", "MIN": 0.02, "MAX": 0.20, "DEFAULT": 0.07},
    {"NAME": "flipAmount", "LABEL": "Band Response", "TYPE": "float", "MIN": 0.0, "MAX": 1.0,  "DEFAULT": 0.7},
    {"NAME": "rotAmount",  "LABEL": "Beat Rotate",   "TYPE": "float", "MIN": 0.0, "MAX": 1.0,  "DEFAULT": 0.6},
    {"NAME": "drift",      "LABEL": "Drift Speed",   "TYPE": "float", "MIN": 0.0, "MAX": 1.0,  "DEFAULT": 0.25},
    {"NAME": "invert",     "LABEL": "Invert",        "TYPE": "float", "MIN": 0.0, "MAX": 1.0,  "DEFAULT": 0.0}
  ]
}*/

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float knee(float x, float lo, float hi) { return clamp(smoothstep(lo, hi, x), 0.0, 1.0); }

// Log-frequency FFT lookup — musical energy lives in the low bins.
float fftLog(float t) { return texture2D(audioFFT, vec2(pow(clamp(t, 0.0, 1.0), 2.2) * 0.5, 0.5)).r; }

// One primitive per cell, drawn in local [-1,1] space. Returns coverage 0..1.
float primitive(vec2 q, float kind, float lw, float aa) {
    float r = length(q);
    if (kind < 1.0) {                    // solid disc
        return smoothstep(aa, -aa, r - 0.62);
    } else if (kind < 2.0) {             // half-moon (disc clipped by its own offset)
        float d1 = smoothstep(aa, -aa, r - 0.62);
        float d2 = smoothstep(aa, -aa, length(q - vec2(0.30, 0.0)) - 0.62);
        return clamp(d1 - d2, 0.0, 1.0);
    } else if (kind < 3.0) {             // quarter arc (ring clipped to a quadrant)
        float ring = smoothstep(aa, -aa, abs(r - 0.55) - lw * 2.4);
        float quad = step(0.0, q.x) * step(0.0, q.y);
        return ring * quad;
    } else if (kind < 4.0) {             // hollow square
        vec2 a = abs(q);
        float box = max(a.x, a.y);
        return smoothstep(aa, -aa, abs(box - 0.55) - lw * 2.2);
    }                                    // diagonal bar
    float d = abs(q.x + q.y) * 0.7071;
    return smoothstep(aa, -aa, d - lw * 2.6);
}

void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / min(RENDERSIZE.x, RENDERSIZE.y);

    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6);
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2);
    float drive = 0.25 + 0.75 * knee(audioEnergy, 0.05, 0.9);
    float levelP = knee(audioLevel, 0.03, 0.8);
    float beat  = clamp(audioBeatPulse, 0.0, 1.0);

    // Bounded phase offset, never TIME*drive — the conveyor drift below
    // multiplies mt into POSITION, so a drive-scaled clock would shift the
    // whole grid sideways on every energy change.
    float mt = TIME * 0.65 + drive * 0.4;

    // The whole plate breathes a whisper with bass — global scale belongs to
    // the lowest band, and Milkdrop taught us ~1% already reads.
    p *= 1.0 - 0.012 * bassP;

    // Slow conveyor drift keeps the grid alive in silence.
    p.x += mt * drift * 0.05;

    float cell = 1.4 / gridSize;
    vec2 gid = floor(p / cell);
    vec2 q = (fract(p / cell) - 0.5) * 2.0;          // local [-1,1]

    float h  = hash21(gid);
    float h2 = hash21(gid + 41.7);
    float h3 = hash21(gid + 113.9);

    // Cell margin: shrink local space so tiles never touch.
    q /= max(1.0 - margin * 2.0, 0.2);

    // Beat rotation: a sparse subset leans toward 90° and eases back through
    // the pulse tail. Per-cell depth (h3) staggers how far each member goes,
    // so the grid never steps as one block in a single frame.
    float member = step(0.85, h3);
    float rot = 1.1 * rotAmount * member * beat * beat * beat * (0.35 + 0.65 * fract(h3 * 7.31));
    // Plus each cell owns a whisper of permanent orientation.
    rot += floor(h2 * 4.0) * 1.5708;
    float cs = cos(rot), sn = sin(rot);
    q = mat2(cs, -sn, sn, cs) * q;

    float aa = gridSize / min(RENDERSIZE.x, RENDERSIZE.y) * 3.0;
    float kind = floor(h * 5.0);
    float shape = primitive(q, kind, lineWeight, aa);

    // Per-cell band listener (golden technique #2): a stable hash-assigned
    // slice of the spectrum decides whether this tile speaks. The wide knee
    // makes it a lean rather than a hard flip — no full-frame flicker.
    float band = fftLog(h);
    float speak = knee(band, 0.15, 0.92) * flipAmount * 0.85;

    // Tiles alternate polarity: some draw white-on-black, others black-on-white.
    float polarity = step(0.5, h2);
    float ink = mix(shape, 1.0 - shape, polarity);

    // A speaking tile inverts — the band flips its figure and ground.
    ink = mix(ink, 1.0 - ink, speak);

    // Highs: hairline crosshair blinks in a very sparse subset of empty cells.
    if (h3 < 0.08) {
        float cross = min(abs(q.x), abs(q.y));
        float hair = smoothstep(aa * 2.0, 0.0, cross - lineWeight * 0.5);
        ink = max(ink, hair * highP * 0.8);
    }

    float v = clamp(ink, 0.0, 1.0) * (0.72 + 0.12 * drive + 0.16 * levelP);

    v = mix(v, 1.0 - v, clamp(invert, 0.0, 1.0));
    gl_FragColor = vec4(vec3(v), 1.0);
}
