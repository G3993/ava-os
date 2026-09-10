/*{
  "DESCRIPTION": "Sphere Swarm — an Unreal-Niagara-style particle formation: a hundred small rounded gradient cards distributed on an invisible fibonacci sphere, perspective-projected onto a clean white void, slowly revolving. Near cards read large and crisp, the far hemisphere shrinks and fades into the paper. Each chip is a two-color gradient cut from a curated palette around the two anchor inks, with a hairline edge. Bass breathes the sphere radius, mids flutter individual card tilt, and every beat sends an eased ripple of brightness rolling across the formation from the north pole.",
  "CREDIT": "ShaderClaw3",
  "CATEGORIES": [
    "Generator",
    "Geometry",
    "3D",
    "Audio Reactive"
  ],
  "INPUTS": [
    {
      "NAME": "colorA",
      "LABEL": "Palette Anchor A",
      "TYPE": "color",
      "DEFAULT": [0.90, 0.30, 0.22, 1.0],
      "GROUP": "Color"
    },
    {
      "NAME": "colorB",
      "LABEL": "Palette Anchor B",
      "TYPE": "color",
      "DEFAULT": [0.16, 0.32, 0.82, 1.0],
      "GROUP": "Color"
    },
    {
      "NAME": "paletteShift",
      "LABEL": "Palette Shift",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 10,
      "DEFAULT": 0,
      "GROUP": "Color"
    },
    {
      "NAME": "brightness",
      "LABEL": "Brightness",
      "TYPE": "float",
      "MIN": 0.2,
      "MAX": 3,
      "DEFAULT": 1,
      "GROUP": "Color"
    },
    {
      "NAME": "cardCount",
      "LABEL": "Cards",
      "TYPE": "float",
      "MIN": 40,
      "MAX": 128,
      "DEFAULT": 122,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "sphereSize",
      "LABEL": "Sphere Size",
      "TYPE": "float",
      "MIN": 0.5,
      "MAX": 1.4,
      "DEFAULT": 1.05,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "spinSpeed",
      "LABEL": "Spin Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 3,
      "DEFAULT": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "flutter",
      "LABEL": "Card Flutter",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.55,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "audioReact",
      "LABEL": "Audio Reactivity",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.6,
      "GROUP": "Audio Reactivity"
    }
  ]
}*/

float hash11(float p) {
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}

vec2 hash21f(float p) {
    return vec2(hash11(p * 1.37 + 3.1), hash11(p * 2.71 + 17.7));
}

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float knee(float x, float lo, float hi) { return smoothstep(lo, hi, x); }

// hue rotation around the grey axis (Rodrigues)
vec3 hueRot(vec3 c, float a) {
    vec3 k = vec3(0.57735);
    float co = cos(a), si = sin(a);
    return c * co + cross(k, c) * si + k * dot(k, c) * (1.0 - co);
}

// Curated chip inks derived from the two anchors — muted studio family plus
// two vivid accents, so the sphere reads designed rather than rainbow.
vec3 chipInk(float h) {
    float k = mod(floor(h * 8.0), 8.0);
    vec3 A = hueRot(colorA.rgb, paletteShift * 0.6283);
    vec3 B = hueRot(colorB.rgb, paletteShift * 0.6283);
    if (k < 0.5) return A;                                   // vivid anchor A
    if (k < 1.5) return B;                                   // vivid anchor B
    if (k < 2.5) return mix(A, vec3(0.98), 0.42);            // light A wash
    if (k < 3.5) return mix(B, vec3(0.98), 0.40);            // light B wash
    if (k < 4.5) return mix(A, vec3(0.06, 0.06, 0.07), 0.60);// deep A shadow
    if (k < 5.5) return mix(B, vec3(0.05, 0.05, 0.08), 0.55);// deep B shadow
    if (k < 6.5) return vec3(0.13, 0.125, 0.125);            // charcoal
    return mix(A, B, 0.5) * 1.15;                            // blended mid ink
}

void main() {
    vec2 uv = isf_FragNormCoord.xy;
    float aspect = RENDERSIZE.x / RENDERSIZE.y;
    vec2 q = (uv - 0.5) * vec2(aspect, 1.0) * 2.0;

    // ── audio conditioning ──
    float aR    = clamp(audioReact, 0.0, 1.0);
    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6);
    float midP  = pow(knee(audioMid,  0.08, 0.85), 1.3);
    float levP  = knee(audioLevel, 0.03, 0.85);
    float bp    = clamp(audioBeatPulse, 0.0, 1.0);
    // music-accumulated spin: the formation revolves faster when the track moves
    float ck    = TIME * spinSpeed + audioTime * 0.55 * aR * spinSpeed;

    // clean white void, whisper of warmth + corner falloff
    vec3 bg = vec3(0.982, 0.979, 0.972);
    bg *= 1.0 - 0.05 * dot(q, q) * 0.25;

    // ── sphere frame ──
    float R = sphereSize * (1.0 + 0.015 * sin(ck * 0.37)
                                + 0.075 * aR * bassP);   // bass breathes radius
    float D = 3.05;                                       // camera distance
    float F = 1.72;                                       // focal scale
    float yaw  = ck * 0.13 + 0.35;
    float tilt = 0.34 + 0.10 * sin(ck * 0.045);
    float cy = cos(yaw),  sy = sin(yaw);
    float ct = cos(tilt), st = sin(tilt);

    // beat ripple front: rolls pole-to-pole as the beat pulse decays
    float rippleT = 1.0 - pow(bp, 0.7);

    vec3  acc   = bg;
    float bestZ = 1e9;
    float aaPx  = 2.4 / RENDERSIZE.y;

    for (int i = 0; i < 128; i++) {
        float fi = float(i);
        if (fi >= cardCount) break;

        // fibonacci sphere point
        float fu = (fi + 0.5) / cardCount;
        float py = 1.0 - 2.0 * fu;
        float pr = sqrt(max(0.0, 1.0 - py * py));
        float th = fi * 2.399963;
        vec3 p = vec3(pr * cos(th), py, pr * sin(th));

        // rotate: yaw about Y, then tilt about X
        p = vec3(p.x * cy + p.z * sy, p.y, -p.x * sy + p.z * cy);
        p = vec3(p.x, p.y * ct - p.z * st, p.y * st + p.z * ct);

        // perspective projection (camera at origin looking +z, sphere at z=D)
        float zv    = D + p.z * R;
        float persp = F / zv;
        vec2  scr   = p.xy * R * persp;

        // per-card identity
        float h1 = hash11(fi * 7.13 + 1.7);
        float h2 = hash11(fi * 3.77 + 9.2);
        float h3 = hash11(fi * 5.31 + 4.4);

        // card half-extents: portrait-leaning chips, size varies per id;
        // the whole formation swells gently with sustained level
        float s = (0.040 + 0.022 * h1) * persp * 2.9
                * (1.0 + 0.10 * aR * levP);
        float arv = 0.52 + 0.30 * h2;                 // width/height ratio
        vec2 he = vec2(s * arv, s);

        // coarse reject before doing card-local work
        vec2 d0 = q - scr;
        if (abs(d0.x) > s * 2.1 || abs(d0.y) > s * 2.1) continue;

        // card-local rotation: gentle per-id lean + mid-driven flutter
        float ang = (h3 - 0.5) * 0.55
                  + flutter * (0.10 + 0.55 * aR * midP) * sin(ck * 1.35 + fi * 2.4);
        float ca = cos(ang), sa = sin(ang);
        vec2 ld = vec2(d0.x * ca - d0.y * sa, d0.x * sa + d0.y * ca);
        // flutter also tips the card in depth — reads as a foreshortening squash
        ld.x /= max(0.55, 0.86 + 0.14 * sin(ck * 0.9 + fi * 3.1));

        // rounded-rect SDF
        float rad = s * 0.16;
        vec2 dd = abs(ld) - he + rad;
        float sd = length(max(dd, 0.0)) + min(max(dd.x, dd.y), 0.0) - rad;

        float depthN = clamp((zv - (D - R)) / (2.0 * R), 0.0, 1.0);
        float aaW = aaPx * (1.0 + 2.6 * depthN);       // far side softens
        float aCov = smoothstep(aaW, -aaW, sd);
        if (aCov < 0.004 || zv >= bestZ) continue;

        // ── chip color: 2-ink vertical gradient + per-card hue lean ──
        float lean = (h3 - 0.5) * 0.55;
        vec3 ink1 = hueRot(chipInk(h1 + 0.13), lean);
        vec3 ink2 = hueRot(chipInk(h2 * 0.97 + 0.61), lean);
        float g = clamp(ld.y / he.y * 0.5 + 0.5, 0.0, 1.0);
        vec3 fill = mix(ink1, ink2, smoothstep(0.15, 0.95, g));
        // abstract "photo content": a soft diagonal band of a third ink
        float bandC = (hash11(fi * 11.3 + 0.9) - 0.5) * 1.2;
        float bandD = (ld.x + ld.y * (0.4 + h2)) / s - bandC;
        float bandM = smoothstep(0.34, 0.08, abs(bandD));
        vec3 ink3 = hueRot(chipInk(h3 * 1.31 + 0.37), -lean * 0.7);
        fill = mix(fill, ink3, bandM * 0.55);
        // soft sheen across the chip
        fill += 0.03 * (1.0 - abs(g * 2.0 - 1.0));

        // hairline edge
        float ring = smoothstep(aaW * 1.6, 0.0, abs(sd + aaW * 1.2));
        fill = mix(fill, vec3(0.14, 0.14, 0.15), ring * 0.35);

        // beat ripple: eased brightness wave sweeping from the north pole
        float sPole = clamp((1.0 - py) * 0.5, 0.0, 1.0);
        float wave = exp(-20.0 * (sPole - rippleT) * (sPole - rippleT));
        fill = mix(fill, fill * 1.65 + 0.22, wave * bp * aR);

        // depth fade: far hemisphere sinks into the paper
        fill = mix(fill, bg, 0.70 * pow(depthN, 1.1));
        float aFade = mix(1.0, 0.34, pow(depthN, 1.3));

        acc = mix(acc, fill, aCov * aFade);
        if (aCov > 0.05) bestZ = zv;
    }

    // fine film grain so the void isn't sterile
    float grain = hash21(uv * RENDERSIZE.xy);
    acc += (grain - 0.5) * 0.034;

    // audio brightness: can dip below 1 so lifts never clamp on white
    float lift = mix(1.0, 0.88 + 0.19 * levP, aR);
    acc *= brightness * lift;

    gl_FragColor = vec4(clamp(acc, 0.0, 1.0), 1.0);
}
