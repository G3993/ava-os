/*{
  "DESCRIPTION": "Volcanic Caldera — 3D raymarched molten lava landscape. Domain-warped FBM terrain with glowing obsidian rock and HDR lava channels flowing through deep rifts. Camera orbits the caldera rim at dusk. LINEAR HDR out, no tonemap.",
  "CREDIT": "ShaderClaw auto-improve 2026-05-12",
  "CATEGORIES": [
    "Generator",
    "3D",
    "Audio Reactive"
  ],
  "INPUTS": [
    {
      "NAME": "terrainRough",
      "LABEL": "Terrain Rough",
      "TYPE": "float",
      "MIN": 1,
      "MAX": 6,
      "DEFAULT": 3.5,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "calderaDepth",
      "LABEL": "Caldera Depth",
      "TYPE": "float",
      "MIN": 0.1,
      "MAX": 1,
      "DEFAULT": 0.55,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "orbitSpeed",
      "LABEL": "Orbit Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.06,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "flowSpeed",
      "LABEL": "Flow Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1.5,
      "DEFAULT": 0.28,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "lavaGlow",
      "LABEL": "Lava Glow HDR",
      "TYPE": "float",
      "MIN": 1,
      "MAX": 6,
      "DEFAULT": 3,
      "GROUP": "Color"
    },
    {
      "NAME": "hueShift",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0,
      "LABEL": "Hue Shift",
      "GROUP": "Color"
    },
    {
      "NAME": "colorBoost",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 1,
      "LABEL": "Color Boost",
      "GROUP": "Color"
    },
    {
      "NAME": "bgColor",
      "TYPE": "color",
      "DEFAULT": [
        0,
        0,
        0,
        0
      ],
      "LABEL": "Background",
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

#define MAX_STEPS  80
#define EPS        0.003
#define PI         3.14159265

float h21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0*f);
    return mix(mix(h21(i), h21(i+vec2(1,0)), f.x),
               mix(h21(i+vec2(0,1)), h21(i+vec2(1,1)), f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 6; i++) {
        v += a * vnoise(p);
        p = p * 2.03 + vec2(13.1, 7.7);
        a *= 0.5;
    }
    return v;
}

float terrainH(vec2 xz) {
    // Domain warp for organic lava-flow topology
    vec2 warp = vec2(
        fbm(xz * terrainRough * 0.45 + vec2(0.0, TIME * flowSpeed * 0.08)),
        fbm(xz * terrainRough * 0.45 + vec2(5.3, TIME * flowSpeed * 0.06 + 2.1))
    );
    vec2 wx = xz + 0.38 * warp;
    float h = fbm(wx * terrainRough);
    // Caldera bowl: subtract radial bowl so center sinks
    float r2 = dot(xz, xz);
    h -= calderaDepth * exp(-r2 * 1.8);
    return h;
}

float mapScene(vec3 p) {
    return p.y - terrainH(p.xz) * 0.9;
}

vec3 terrainNormal(vec3 p) {
    const vec2 e = vec2(0.009, 0.0);
    return normalize(vec3(
        mapScene(p+e.xyy) - mapScene(p-e.xyy),
        mapScene(p+e.yxy) - mapScene(p-e.yxy),
        mapScene(p+e.yyx) - mapScene(p-e.yyx)
    ));
}

// Lava palette: obsidian → deep crimson → orange → gold → white-hot
vec3 lavaPalette(float t) {
    t = clamp(t, 0.0, 1.0);
    if (t < 0.25) return mix(vec3(0.02, 0.01, 0.01), vec3(0.50, 0.04, 0.02), t/0.25);
    if (t < 0.55) return mix(vec3(0.50, 0.04, 0.02), vec3(1.0,  0.22, 0.0),  (t-0.25)/0.30);
    if (t < 0.82) return mix(vec3(1.0,  0.22, 0.0),  vec3(1.1,  0.75, 0.0),  (t-0.55)/0.27);
    return mix(vec3(1.1, 0.75, 0.0), vec3(2.4, 1.9, 1.1), (t-0.82)/0.18);
}

void main() {
    vec2 uv = (gl_FragCoord.xy / RENDERSIZE.xy) * 2.0 - 1.0;
    uv.x *= RENDERSIZE.x / RENDERSIZE.y;

    float bass = clamp(audioBass, 0.0, 1.0) * audioReact;
    float mid  = clamp(audioMid,  0.0, 1.0) * audioReact;
    // Soft-knee conditioned bands (playbook): low floor so sparse/soft hits
    // (hiphop/jazz) still register, headroom so EDM doesn't peg a clamp.
    float bassK = pow(smoothstep(0.04, 0.92, clamp(audioBass, 0.0, 1.0)), 1.3) * audioReact;
    float midK  = smoothstep(0.06, 0.90, clamp(audioMid, 0.0, 1.0)) * audioReact;
    float beatK = audioBeatPulse * audioReact;

    // Calm caldera-rim orbit camera
    float ang = TIME * orbitSpeed;
    vec3 ro = vec3(sin(ang)*2.2, 1.0 + 0.15*sin(TIME*0.18), cos(ang)*2.2);
    vec3 target = vec3(0.0, 0.1, 0.0);
    vec3 fwd   = normalize(target - ro);
    vec3 rgt   = normalize(cross(vec3(0,1,0), fwd));
    vec3 upV   = cross(fwd, rgt);
    vec3 rd    = normalize(fwd + rgt*uv.x*0.75 + upV*uv.y*0.75);

    // Terrain raymarch
    float t = 0.05;
    bool hit = false;
    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 p = ro + rd * t;
        float d = mapScene(p);
        if (d < EPS) { hit = true; break; }
        if (t > 7.0) break;
        t += max(d * 0.75, EPS * 2.0);
    }

    vec3 col = vec3(0.0);

    if (hit) {
        vec3 p = ro + rd * t;
        vec3 n = terrainNormal(p);
        float h = terrainH(p.xz);

        // Lava intensity: low spots glow brightest. Bass WIDENS the lava
        // channels (spatial breathing — can't saturate at the display clamp
        // the way brightness on already-HDR lava does).
        float lavaT = 1.0 - smoothstep(0.05, 0.45,
                          h + calderaDepth * 0.6 - 0.13 * bassK - 0.05 * midK);
        lavaT = pow(lavaT, 1.4);

        // Pulsing with TIME and audio-bass
        float pulse = 0.82 + 0.18*sin(TIME*2.3 + fbm(p.xz*3.5)*6.28)
                     + bass * (1.0 + 0.5*lavaT) * 0.55;
        vec3 lavaCol = lavaPalette(clamp(lavaT * pulse, 0.0, 1.0)) * lavaGlow;

        // Obsidian rock: dark specular highlight from a warm overhead sun
        vec3 lSun    = normalize(vec3(0.3, 1.8, 0.5));
        float dSun   = max(dot(n, lSun), 0.0);
        vec3 viewV   = -rd;
        vec3 halfSun = normalize(lSun + viewV);
        float spec   = pow(max(dot(n, halfSun), 0.0), 80.0);
        vec3 rockCol = vec3(0.04, 0.025, 0.02) + vec3(0.8, 0.35, 0.1)*spec*1.5*dSun;

        col = mix(rockCol, lavaCol, smoothstep(0.0, 0.35, lavaT));

        // Edge darkening at height-gradient seams (fwidth AA)
        float edgeMask = clamp(fwidth(h) * 160.0, 0.0, 1.0);
        col *= 1.0 - edgeMask * 0.40;

        // Atmospheric haze (crimson smoke near surface) — mids warm the haze
        // (continuous band-following on the dark rock regions, has headroom).
        float fog = exp(-t * 0.15);
        col = mix(vec3(0.14, 0.04, 0.02) * (1.0 + 1.1 * midK + 0.6 * beatK), col, fog);
    } else {
        // Ashen caldera sky: charcoal to deep crimson at horizon
        float skyT  = clamp(uv.y * 0.5 + 0.5, 0.0, 1.0);
        col = mix(vec3(0.24, 0.06, 0.02), vec3(0.06, 0.04, 0.07), skyT);
        // Lava-lake uplight glow at horizon — breathes with bass so sky
        // pixels respond too (the raymarch miss half of the frame).
        col += vec3(0.40, 0.13, 0.02) * exp(-skyT * skyT * 5.0)
             * (1.0 + 0.9 * bassK + 0.5 * midK);
        // universal background override — sky/miss region (a=0 -> untouched)
        col = mix(col, bgColor.rgb, bgColor.a);
    }

    // Global emissive breath from lava lake (audio-modulated, whole frame)
    col += vec3(0.045, 0.012, 0.0) * (0.6 + 0.4*sin(TIME*0.45))
         * (1.0 + mid*2.2 + bassK*2.0 + beatK*1.5);

    // ---- universal color block (defaults = no-op) ----
    float ucL = dot(col, vec3(0.299, 0.587, 0.114));
    vec3 uc = mix(vec3(ucL), col, colorBoost);
    if (hueShift > 0.0005) {
        float hueA = hueShift * 6.2831853;
        float hueC = cos(hueA), hueS = sin(hueA);
        mat3 hueM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
                  + hueC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
                  + hueS * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
        uc = clamp(hueM * uc, 0.0, 1.0);
    }

    // LINEAR HDR — no tonemap, no clamp
    gl_FragColor = vec4(uc, 1.0);
}
