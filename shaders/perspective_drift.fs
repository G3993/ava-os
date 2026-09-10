/*{
  "DESCRIPTION": "Perspective Drift — a one-point-perspective chamber of fine hairline grid lines on warm paper, converging to a deep hazed vanishing point. Translucent frosted planes in coral, cobalt, amber and sage drift forward through the room, casting soft shadows onto the grid, swelling gently as they near the eye and dissolving into the atmosphere far away. The camera glides forward forever on a music-time clock: energy drives the glide, bass swells the nearest plane, and each beat spawns a fresh plane deep in the haze that sails toward the viewer.",
  "CREDIT": "ShaderClaw3",
  "CATEGORIES": [
    "Generator",
    "Geometry",
    "3D",
    "Audio Reactive"
  ],
  "INPUTS": [
    {
      "NAME": "roomTint",
      "LABEL": "Room Tint",
      "TYPE": "color",
      "DEFAULT": [0.956, 0.936, 0.892, 1.0],
      "GROUP": "Color"
    },
    {
      "NAME": "planeInk",
      "LABEL": "Plane Ink Anchor",
      "TYPE": "color",
      "DEFAULT": [0.88, 0.40, 0.30, 1.0],
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
      "NAME": "gridDensity",
      "LABEL": "Grid Density",
      "TYPE": "float",
      "MIN": 2,
      "MAX": 12,
      "DEFAULT": 7,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "planeCount",
      "LABEL": "Planes",
      "TYPE": "float",
      "MIN": 3,
      "MAX": 10,
      "DEFAULT": 8,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "glideSpeed",
      "LABEL": "Glide Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 3,
      "DEFAULT": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "hazeAmount",
      "LABEL": "Depth Haze",
      "TYPE": "float",
      "MIN": 0.2,
      "MAX": 2,
      "DEFAULT": 1,
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

float hash21(vec2 p) {
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float knee(float x, float lo, float hi) { return smoothstep(lo, hi, x); }

vec3 hueRot(vec3 c, float a) {
    vec3 k = vec3(0.57735);
    float co = cos(a), si = sin(a);
    return c * co + cross(k, c) * si + k * dot(k, c) * (1.0 - co);
}

// curated frosted inks: coral / cobalt / amber / sage, pulled toward the anchor
vec3 planeColor(float h) {
    float k = mod(floor(h * 4.0), 4.0);
    vec3 c;
    if      (k < 0.5) c = vec3(0.90, 0.41, 0.32);   // coral
    else if (k < 1.5) c = vec3(0.25, 0.37, 0.78);   // cobalt
    else if (k < 2.5) c = vec3(0.93, 0.71, 0.28);   // amber
    else              c = vec3(0.60, 0.70, 0.53);   // sage
    c = mix(c, planeInk.rgb, 0.28);
    return hueRot(c, paletteShift * 0.6283);
}

// rounded-rect SDF
float rrect(vec2 d, vec2 he, float r) {
    vec2 dd = abs(d) - he + r;
    return length(max(dd, 0.0)) + min(max(dd.x, dd.y), 0.0) - r;
}

void main() {
    vec2 uv = isf_FragNormCoord.xy;
    float aspect = RENDERSIZE.x / RENDERSIZE.y;
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= aspect;

    // ── audio conditioning ──
    float aR    = clamp(audioReact, 0.0, 1.0);
    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6);
    float levP  = knee(audioLevel, 0.03, 0.85);
    float drive = 0.25 + 0.75 * knee(audioEnergy, 0.05, 0.9);
    float bp    = clamp(audioBeatPulse, 0.0, 1.0);

    // camera glide: baseline drift + music-accumulated time, energy scales it
    float camZ = glideSpeed * (0.45 * TIME + 0.85 * audioTime * aR) * (0.6 + 0.4 * drive);

    // ── room geometry: walls at x=±aspect, floor/ceiling at y=±1 ──
    float kx = abs(p.x) / aspect;
    float ky = abs(p.y);
    float k  = max(kx, ky);
    k = max(k, 1e-4);
    float z = 1.0 / k;                       // depth of the hit point

    // surface coordinates for the grid
    float lateral = (kx > ky) ? p.y * z      // side wall: vertical coord
                              : p.x * z;     // floor/ceiling: horizontal coord
    vec2 g = vec2(z + camZ, lateral * (kx > ky ? 1.0 : aspect));
    g *= gridDensity * 0.5;

    // warm paper room, planes shaded per face
    vec3 paper = roomTint.rgb;
    float faceShade = (kx > ky)
        ? 0.965                              // walls a touch dimmer
        : ((p.y < 0.0) ? 0.935 : 1.005);     // floor darkest, ceiling lightest
    vec3 col = paper * faceShade;

    // atmospheric haze toward the vanishing point
    float fog = exp(-z * 0.14 / hazeAmount);
    vec3 hazeCol = paper * 0.998;

    // ── crisp AA hairline grid ──
    vec2 gw = fwidth(g);
    vec2 ga = abs(fract(g + 0.5) - 0.5);
    vec2 gl = vec2(smoothstep(gw.x * 1.4 + 0.012, gw.x * 0.4, ga.x),
                   smoothstep(gw.y * 1.4 + 0.012, gw.y * 0.4, ga.y));
    float line = max(gl.x, gl.y);
    // moire guard: let lines dissolve when denser than pixels
    line *= smoothstep(0.9, 0.25, max(gw.x, gw.y));
    vec3 inkGrid = mix(paper * 0.44, paper * 0.24, 0.5);
    float fogG = exp(-z * 0.10 / hazeAmount);   // grid survives deeper than air
    col = mix(col, inkGrid, line * 0.72 * fogG);

    // corner seams — four hairlines converging on the vanishing point
    float seam = abs(kx - ky) / max(fwidth(kx - ky), 1e-4);
    col = mix(col, paper * 0.72, (1.0 - min(seam * 0.5, 1.0)) * 0.35 * fog);

    // sink the room into haze
    col = mix(hazeCol, col, fog);

    // ── translucent frosted planes drifting through depth ──
    float L = 5.5;                            // depth recycle length
    float aaS = 2.0 / RENDERSIZE.y;
    for (int i = 0; i < 11; i++) {
        float fi = float(i);
        bool isBeatPlane = (i == 10);
        if (!isBeatPlane && fi >= planeCount) continue;

        float d, aScale = 1.0;
        float seed;
        if (isBeatPlane) {
            // beat plane: born deep in the haze at the hit, sails forward as
            // the pulse decays, dissolving before it reaches the eye
            float e = bp * (2.0 - bp);        // ease-out
            d = mix(2.4, 13.0, e);
            aScale = smoothstep(0.03, 0.30, bp) * aR;
            if (aScale < 0.01) continue;
            seed = 77.7;
        } else {
            // stratified through depth so the parade never bunches up
            float off = (fi + 0.15 + 0.7 * hash11(fi * 9.17 + 2.3)) / planeCount * L;
            d = mod(off - camZ, L) + 0.95;
            // respawn with a new identity each cycle, always far away
            float cyc = floor((camZ - off + L * 400.0) / L);
            seed = fi * 7.3 + cyc * 13.71;
        }

        float h1 = hash11(seed + 0.7);
        float h2 = hash11(seed * 1.61 + 5.1);
        float h3 = hash11(seed * 2.23 + 8.9);

        // world placement — generous panes spread wall-to-wall
        vec2 whe = vec2(0.38 + 0.34 * h3, 0.32 + 0.30 * hash11(seed + 3.3));
        vec2 wc = vec2((h1 - 0.5) * 2.0 * (1.02 * aspect - whe.x),
                       (h2 - 0.5) * 2.0 * (1.02 - whe.y));
        // slow lateral drift keeps the parade from clumping dead-center
        wc.x += 0.15 * sin(TIME * 0.085 + fi * 2.7);
        wc.y += 0.10 * sin(TIME * 0.067 + fi * 1.9);
        if (isBeatPlane) { wc *= 0.5; wc.x += 0.3 * sin(TIME * 0.07); }

        // bass swells the nearest planes
        float nearW = smoothstep(6.0, 1.4, d);
        whe *= 1.0 + 0.10 * aR * bassP * nearW + 0.012 * sin(TIME * 0.5 + fi);

        // project to screen
        vec2 sc  = wc / d;
        vec2 she = whe / d;
        vec2 dd  = p - sc;

        float planeFog = exp(-max(d - 2.2, 0.0) * 0.09 / hazeAmount)
                       * smoothstep(6.4, 3.6, d)   // deep planes dissolve into haze
                       * smoothstep(0.95, 1.25, d)  // never pops at the lens
                       * aScale;
        if (planeFog < 0.01) continue;

        // soft shadow cast onto the grid behind (order-independent multiply)
        float sdS = rrect(dd - vec2(0.045, -0.075) / d, she, 0.02 / d);
        float aSh = smoothstep(0.05 / d + 0.015, -0.008, sdS);
        col *= 1.0 - mix(0.22, 0.08, smoothstep(2.0, 8.0, d)) * aSh * planeFog;

        // frosted pane: multiplicative tint + faint lift, hairline rim
        float sd = rrect(dd, she, 0.045 / d);
        float aP = smoothstep(aaS * (1.0 + d * 0.4), -aaS, sd);
        vec3 tint = planeColor(h1 * 0.93 + h3 * 0.31);
        float aFin = aP * 0.80 * planeFog;
        col *= mix(vec3(1.0), tint, aFin * 0.92);
        col += tint * aFin * 0.16;
        // rim highlight — frosted glass edge catching the room light
        float rim = smoothstep(aaS * 2.6, 0.0, abs(sd)) * planeFog;
        col += rim * 0.10 * mix(vec3(1.0), tint, 0.4) * aP;
        col = mix(col, mix(col, tint * 1.06, 0.5), rim * 0.25);
    }

    // paper grain + gentle vignette
    float grain = hash21(uv * RENDERSIZE.xy);
    col += (grain - 0.5) * 0.024;
    col *= 1.0 - 0.13 * dot(uv - 0.5, uv - 0.5) * 2.2;

    // audio brightness lift that can dip below 1
    float lift = mix(1.0, 0.89 + 0.15 * levP, aR);
    col *= brightness * lift;

    gl_FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
