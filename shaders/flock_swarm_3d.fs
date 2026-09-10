/*{
  "DESCRIPTION": "Raymarched 3D swarm of 8 glowing metaball creatures driven by a genuine boids flocking simulation (cohesion/separation/alignment) held in a persistent per-agent state buffer. An idle wander drive keeps the flock alive and organic even in total silence; audio nudges cohesion (bass makes the flock clump/fuse), separation/scatter (highs dart the flock apart) and overall flight energy (mids), with a synchronized glow flash across the whole flock on the beat.",
  "CREDIT": "ShaderClaw3",
  "CATEGORIES": [
    "Generator",
    "3D",
    "Audio Reactive"
  ],
  "INPUTS": [
    {
      "NAME": "glowIntensity",
      "LABEL": "Glow",
      "TYPE": "float",
      "DEFAULT": 1,
      "MIN": 0,
      "MAX": 2
    },
    {
      "NAME": "texTint",
      "LABEL": "Image Tint Amount",
      "TYPE": "float",
      "DEFAULT": 0.4,
      "MIN": 0,
      "MAX": 1
    },
    {
      "NAME": "inputImage",
      "TYPE": "image",
      "LABEL": "Input Image"
    },
    {
      "NAME": "creatureSize",
      "LABEL": "Creature Size",
      "TYPE": "float",
      "DEFAULT": 0.16,
      "MIN": 0.05,
      "MAX": 0.35,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "cohesionStrength",
      "LABEL": "Cohesion",
      "TYPE": "float",
      "DEFAULT": 0.95,
      "MIN": 0,
      "MAX": 3,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "separationStrength",
      "LABEL": "Separation",
      "TYPE": "float",
      "DEFAULT": 1.7,
      "MIN": 0,
      "MAX": 3,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "alignStrength",
      "LABEL": "Alignment",
      "TYPE": "float",
      "DEFAULT": 1,
      "MIN": 0,
      "MAX": 3,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "wanderAmt",
      "LABEL": "Idle Wander",
      "TYPE": "float",
      "DEFAULT": 0.55,
      "MIN": 0,
      "MAX": 1.5,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "primaryColor",
      "LABEL": "Primary Hue",
      "TYPE": "color",
      "DEFAULT": [
        0.15,
        0.85,
        0.95,
        1
      ],
      "GROUP": "Color"
    },
    {
      "NAME": "accentColor",
      "LABEL": "Accent Hue",
      "TYPE": "color",
      "DEFAULT": [
        0.8,
        0.3,
        0.95,
        1
      ],
      "GROUP": "Color"
    },
    {
      "NAME": "hueShift",
      "LABEL": "Hue Shift",
      "TYPE": "float",
      "DEFAULT": 0,
      "MIN": 0,
      "MAX": 1,
      "GROUP": "Color"
    },
    {
      "NAME": "colorBoost",
      "LABEL": "Color Boost",
      "TYPE": "float",
      "DEFAULT": 1,
      "MIN": 0,
      "MAX": 2,
      "GROUP": "Color"
    },
    {
      "NAME": "camDist",
      "LABEL": "Camera Distance",
      "TYPE": "float",
      "DEFAULT": 2.7,
      "MIN": 1.4,
      "MAX": 5,
      "GROUP": "Camera / Layout"
    },
    {
      "NAME": "camOrbitSpeed",
      "LABEL": "Camera Orbit Speed",
      "TYPE": "float",
      "DEFAULT": 0.5,
      "MIN": 0,
      "MAX": 2,
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
      "LABEL": "Sound Reactivity",
      "TYPE": "float",
      "DEFAULT": 1,
      "MIN": 0,
      "MAX": 2,
      "GROUP": "Audio Reactivity"
    }
  ],
  "PASSES": [
    {
      "TARGET": "agentBuf",
      "PERSISTENT": true,
      "WIDTH": "8",
      "HEIGHT": "1"
    },
    {}
  ]
}*/

// ---------------------------------------------------------------------------
// Flock Swarm 3D — 8 boids-simulated creatures, raymarched as glowing
// anisotropic metaballs. Pass 0 integrates real flocking dynamics into a
// persistent 8x1 state buffer (pos.xy + vel.xy per agent, one texel each).
// Pass 1 raymarches the reconstructed 3D scene and shades it.
// ---------------------------------------------------------------------------

#define WORLD_R    1.4
#define ENC_R      1.9
#define MAX_SPEED  1.1
#define ENC_V      2.0
#define SEP_RADIUS 0.42

// ---- basic hashes ----------------------------------------------------------
float hash11(float p){
    p = fract(p * 0.1031);
    p *= p + 33.33;
    p *= p + p;
    return fract(p);
}
vec2 hash21(float p){
    vec3 p3 = fract(vec3(p) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.xx + p3.yz) * p3.zy);
}

// ---- audio conditioning (house playbook) -----------------------------------
float knee(float x, float lo, float hi){ return smoothstep(lo, hi, x); }

// ---- agent-buffer encode / decode ------------------------------------------
// Position and velocity are packed 0..1 per channel so the persistent buffer
// (an 8x1 RGBA target) can round-trip through the host's texture storage.
vec2 encodePos(vec2 p){
    p = clamp(p, vec2(-ENC_R), vec2(ENC_R));
    return p / ENC_R * 0.5 + 0.5;
}
vec2 decodePos(vec2 e){
    return (e - 0.5) * 2.0 * ENC_R;
}
vec2 encodeVel(vec2 v){
    float l = length(v);
    if (l > ENC_V) v = v / l * ENC_V;
    return v / ENC_V * 0.5 + 0.5;
}
vec2 decodeVel(vec2 e){
    return (e - 0.5) * 2.0 * ENC_V;
}

// One texel == one agent. idx is always a literal 0..7 at every call site
// (unrolled by the constant-bound for-loops below), so this is a plain
// texture2D lookup, not dynamic array indexing.
vec4 agentState(int idx){
    return texture2D(agentBuf, vec2((float(idx) + 0.5) / 8.0, 0.5));
}

// Gentle per-agent vertical bob so the flock reads as fully 3D without
// needing to simulate/store a height channel. Purely time-driven (alive in
// silence); mids nudge the bob rate slightly for extra life on musical energy.
float bobHeight(float idx){
    float ph = hash11(idx * 3.1 + 2.0) * 6.2831853;
    float fr = 0.55 + 0.45 * hash11(idx * 5.7 + 9.0);
    fr *= (1.0 + 0.25 * knee(audioMid, 0.05, 0.85) * audioReact);
    return sin(TIME * fr + ph) * 0.22;
}

// Per-agent radius: gentle size variance + idle "breathing" pulse so no two
// creatures are identical and none of them ever look frozen.
float creatureRadius(float idx){
    float rv = 0.82 + 0.36 * hash11(idx * 4.4 + 3.0);
    float breathe = 1.0 + 0.05 * sin(TIME * (0.8 + 0.3 * hash11(idx * 2.2 + 7.0)) + idx * 1.7);
    return creatureSize * rv * breathe;
}

// ---- SDF plumbing -----------------------------------------------------------
float smin(float a, float b, float k){
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

vec3 buildRight(vec3 fwd){
    vec3 upGuess = vec3(0.0, 1.0, 0.0);
    vec3 c = cross(upGuess, fwd);
    float l = length(c);
    return (l < 0.001) ? vec3(1.0, 0.0, 0.0) : (c / l);
}

// Anisotropic ellipsoid (IQ-style bound approximation), stretched along dir
// so a moving creature reads as a body with motion, not a static ball.
float sdEllipsoidAniso(vec3 p, vec3 center, vec3 dir, float baseR, float stretch){
    vec3 d = p - center;
    float dl = length(dir);
    vec3 fwd = (dl < 0.001) ? vec3(0.0, 0.0, 1.0) : (dir / dl);
    vec3 rgt = buildRight(fwd);
    vec3 up  = cross(fwd, rgt);
    vec3 local = vec3(dot(d, rgt), dot(d, up), dot(d, fwd));
    vec3 radii = vec3(baseR, baseR * 0.92, baseR * stretch);
    float k0 = length(local / radii);
    float k1 = length(local / (radii * radii));
    return k0 * (k0 - 1.0) / max(k1, 0.0001);
}

// Union of all 8 creatures, smoothly blended (k grows with bass -> the flock
// visually fuses into a glowing mass when it clumps on a bass hit).
float mapScene(vec3 p, float k){
    float d = 1.0e5;
    for (int i = 0; i < 8; i++){
        float idxf = float(i);
        vec4 st = agentState(i);
        vec2 pos2 = decodePos(st.rg);
        vec2 vel2 = decodeVel(st.ba);
        float by = bobHeight(idxf);
        vec3 center = vec3(pos2.x, by, pos2.y);
        vec3 dir3 = vec3(vel2.x, 0.0, vel2.y);
        float spd = length(vel2);
        float stretch = 1.0 + clamp(spd / MAX_SPEED, 0.0, 1.0) * 1.1;
        float di = sdEllipsoidAniso(p, center, dir3, creatureRadius(idxf), stretch);
        d = smin(d, di, k);
    }
    return d;
}

vec3 calcNormal(vec3 p, float k){
    vec2 e = vec2(0.0015, 0.0);
    return normalize(vec3(
        mapScene(p + e.xyy, k) - mapScene(p - e.xyy, k),
        mapScene(p + e.yxy, k) - mapScene(p - e.yxy, k),
        mapScene(p + e.yyx, k) - mapScene(p - e.yyx, k)
    ));
}

// Per-agent skin tint: curated 2-hue family (primary/accent) with a small
// spectral shimmer from the audio palette, then meaningfully re-tinted by the
// image input (sampled per-agent-index, so the flock visibly varies skin).
vec3 creatureColor(int idx){
    float idxf = float(idx);
    float hueMixT = hash11(idxf * 1.7 + 0.33);
    vec3 base = mix(primaryColor.rgb, accentColor.rgb, hueMixT);
    base = mix(base, audioPalAccent, 0.12 * knee(audioHigh, 0.1, 0.85) * audioReact);
    vec3 texCol = base;
    if (IMG_SIZE_inputImage.x > 0.0){
        texCol = texture2D(inputImage, vec2((idxf + 0.5) / 8.0, 0.5)).rgb;
    }
    return mix(base, texCol, texTint);
}

// Smooth metaball-style color blend at a hit point (creatures fused by smin
// blend their colors too, not just their shapes).
vec3 blendedColorAt(vec3 p){
    vec3 colSum = vec3(0.0);
    float wSum = 0.0;
    for (int i = 0; i < 8; i++){
        float idxf = float(i);
        vec4 st = agentState(i);
        vec2 pos2 = decodePos(st.rg);
        vec2 vel2 = decodeVel(st.ba);
        float by = bobHeight(idxf);
        vec3 center = vec3(pos2.x, by, pos2.y);
        vec3 dir3 = vec3(vel2.x, 0.0, vel2.y);
        float spd = length(vel2);
        float stretch = 1.0 + clamp(spd / MAX_SPEED, 0.0, 1.0) * 1.1;
        float di = sdEllipsoidAniso(p, center, dir3, creatureRadius(idxf), stretch);
        float w = exp(-max(di, 0.0) * 26.0);
        colSum += creatureColor(i) * w;
        wSum += w;
    }
    return colSum / max(wSum, 0.0001);
}

// ---------------------------------------------------------------------------
// PASS 0 : boids simulation -> agentBuf (8x1, one texel per agent)
// ---------------------------------------------------------------------------
void simPass(){
    float agentIdx = floor(gl_FragCoord.x);

    vec4 raw = texture2D(agentBuf, vec2((agentIdx + 0.5) / 8.0, 0.5));
    vec2 myPos = decodePos(raw.rg);
    vec2 myVel = decodeVel(raw.ba);

    // Warmup: seed agents spread out on a ring with small random headings —
    // never all at the origin, so the very first frames already fly apart.
    if (FRAMEINDEX < 2){
        vec2 h1 = hash21(agentIdx * 7.13 + 1.0);
        float ang = h1.x * 6.2831853;
        float rad = (0.25 + 0.55 * h1.y) * WORLD_R;
        myPos = vec2(cos(ang), sin(ang)) * rad;
        vec2 h2 = hash21(agentIdx * 3.37 + 5.0);
        float vang = h2.x * 6.2831853;
        myVel = vec2(cos(vang), sin(vang)) * (0.35 + 0.35 * h2.y);
    }

    // Standard conditioning snippet (house playbook), extended per-band.
    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6) * audioReact;
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2) * audioReact;
    float midP  = knee(audioMid, 0.05, 0.90) * audioReact;
    float drive = 0.25 + 0.75 * knee(audioEnergy, 0.05, 0.9);

    // Routing: bass -> cohesion (clump/fuse), highs -> separation (scatter),
    // mid -> flight speed / alignment strength. Never overrides position —
    // it only reweights the boids' own steering forces (law 5).
    float cohesionK   = cohesionStrength   * (1.0 + 1.1 * bassP);
    float separationK = separationStrength * (1.0 + 1.6 * highP);
    float alignK      = alignStrength      * (1.0 + 0.6 * midP);
    float speedMul     = 1.0 + 0.55 * midP;

    vec2 sumPos = vec2(0.0);
    vec2 sumVel = vec2(0.0);
    vec2 sep    = vec2(0.0);

    // Unrolled (constant bound) read of all 8 agents' state — legal on
    // GLSL ES 1.0 since the loop bound is a literal and the texture
    // coordinate is computed, not used as a dynamic array index.
    for (int i = 0; i < 8; i++){
        float fi = float(i);
        vec4 st = texture2D(agentBuf, vec2((fi + 0.5) / 8.0, 0.5));
        vec2 p = decodePos(st.rg);
        vec2 v = decodeVel(st.ba);
        float notSelf = step(0.5, abs(fi - agentIdx));
        sumPos += p * notSelf;
        sumVel += v * notSelf;
        vec2 d = myPos - p;
        float dist = length(d);
        float w = notSelf * (1.0 - smoothstep(0.0, SEP_RADIUS, dist));
        sep += (d / max(dist, 0.0001)) * w;
    }

    vec2 avgPos = sumPos / 7.0;
    vec2 avgVel = sumVel / 7.0;

    vec2 cohesionF = (avgPos - myPos) * cohesionK;
    vec2 alignF    = (avgVel - myVel) * alignK;
    vec2 sepF      = sep * separationK;

    // Idle wander drive — per-agent independent phase so the flock keeps
    // living, shifting dynamics of its own even in total silence (the
    // sound-off test this shader is built around).
    float wp = agentIdx * 2.399 + 1.0;
    vec2 wander = vec2(cos(TIME * 0.35 + wp), sin(TIME * 0.29 + wp * 1.7))
                  * wanderAmt * (0.6 + 0.4 * drive);

    // Soft bounding wall so the flock stays roughly on-camera without ever
    // hard-clamping position (keeps motion physical).
    float distC = length(myPos);
    vec2 wallF = vec2(0.0);
    if (distC > WORLD_R){
        wallF = -normalize(myPos) * (distC - WORLD_R) * 2.2;
    }

    vec2 accel = cohesionF + alignF + sepF + wander + wallF;

    float dt = clamp(TIMEDELTA, 0.0, 0.05);
    if (dt <= 0.0) dt = 0.016;

    vec2 newVel = myVel + accel * dt;

    float maxV = MAX_SPEED * speedMul;
    float vlen = length(newVel);
    if (vlen > maxV) newVel = newVel / vlen * maxV;

    // Never let an agent fully stall — a flock that stops reads as dead.
    float minV = MAX_SPEED * 0.22;
    vlen = length(newVel);
    if (vlen < minV){
        vec2 fallbackDir = (vlen < 0.0001) ? vec2(cos(wp), sin(wp)) : (newVel / vlen);
        newVel = fallbackDir * minV;
    }

    vec2 newPos = myPos + newVel * dt;

    gl_FragColor = vec4(encodePos(newPos), encodeVel(newVel));
}

// ---------------------------------------------------------------------------
// SCREEN PASS : raymarch the 8 boids as glowing 3D creatures
// ---------------------------------------------------------------------------
void screenPass(){
    vec2 res = RENDERSIZE;
    vec2 ndc = (gl_FragCoord.xy - 0.5 * res) / res.y;

    // Lower sensitivity floor (jazz fix): soft swing accents sit ~0.3-0.5,
    // so the knee starts at 0.03 with a gentler curve than the sim pass.
    float bassP = pow(knee(audioBass, 0.03, 0.80), 1.3) * audioReact;
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2) * audioReact;
    float midP  = knee(audioMid, 0.04, 0.90) * audioReact;
    float drive = 0.25 + 0.75 * knee(audioEnergy, 0.05, 0.9);

    // Track the flock's rough flockCenter so the camera keeps it framed.
    vec2 flockCenter = vec2(0.0);
    for (int i = 0; i < 8; i++){
        vec4 st = agentState(i);
        flockCenter += decodePos(st.rg);
    }
    flockCenter /= 8.0;

    // --- camera: slow orbit around the flock, gently eased by energy -------
    float camAngle = TIME * 0.14 * camOrbitSpeed * (0.8 + 0.3 * drive);
    vec3 ta = vec3(flockCenter.x, 0.05, flockCenter.y);
    vec3 ro = ta + vec3(sin(camAngle), 0.0, cos(camAngle)) * camDist;
    ro.y += 0.55 + 0.15 * sin(TIME * 0.2);

    vec3 fwd = normalize(ta - ro);
    vec3 rgt = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 upv = cross(fwd, rgt);
    float fov = 1.15;
    vec3 rd = normalize(fwd + (ndc.x * rgt + ndc.y * upv) * fov);

    // Blend radius grows with bass -> creatures visibly fuse into one
    // glowing mass on a bass hit, then relax apart as it decays.
    float k = 0.05 + 0.16 * bassP;

    vec3 col = vec3(0.0); // black void
    float glow = 0.0;

    float t = 0.05;
    bool hit = false;
    vec3 hitP = vec3(0.0);

    for (int i = 0; i < 72; i++){
        vec3 p = ro + rd * t;
        float d = mapScene(p, k);
        glow += exp(-abs(d) * 9.0) * 0.030;
        if (d < 0.0015){
            hit = true;
            hitP = p;
            break;
        }
        t += max(d * 0.8, 0.004);
        if (t > 9.0) break;
    }

    // Jazz fix: squaring crushed soft accents (0.45^2 ~ 0.2). A 1.3 exponent
    // plus punch coupling lets quiet swing hits leave a visible decaying
    // flash (both envelopes already decay — never a raw-gate strobe).
    float beatFlash = pow(clamp(max(audioBeatPulse, audioPunch * 0.8), 0.0, 1.0), 1.3) * audioReact;

    if (hit){
        vec3 n = calcNormal(hitP, k);
        vec3 viewDir = normalize(ro - hitP);
        vec3 baseCol = blendedColorAt(hitP);

        vec3 lightDir = normalize(vec3(0.4, 0.8, 0.5));
        float diff = clamp(dot(n, lightDir), 0.15, 1.0);
        float fres = pow(1.0 - clamp(dot(n, viewDir), 0.0, 1.0), 3.0);

        float emissive = 0.32 + 0.32 * fres + beatFlash * 0.55;
        col = baseCol * diff + baseCol * emissive * glowIntensity;
    }

    // Soft additive atmosphere glow — the whole flock breathes light even
    // where the ray doesn't hit a surface, brightening on bass and flashing
    // together on the beat (synchronized flock-wide flash, event-only).
    vec3 glowColor = mix(primaryColor.rgb, accentColor.rgb, 0.5);
    col += glowColor * glow * glowIntensity * (0.5 + 0.6 * bassP + 0.35 * midP) * (1.0 + 1.4 * beatFlash);

    // Luminance-preserving tonemap (per-channel Reinhard desaturates bright
    // highlights toward white; scaling by a single luminance-derived factor
    // keeps the curated hue family reading as high-contrast color, not pastel).
    float lum = dot(col, vec3(0.299, 0.587, 0.114));
    col *= 1.0 / (1.0 + lum);
    col = pow(max(col, 0.0), vec3(1.0 / 2.2));

    // Saturation lift for punch against the black void — counteracts the
    // channel-spread compression that display gamma inflicts on colors with
    // an uneven linear RGB spread (keeps the curated hue read as vivid,
    // not pastel).
    float lum2 = dot(col, vec3(0.299, 0.587, 0.114));
    col = clamp(mix(vec3(lum2), col, 1.9), 0.0, 1.0);

    // ---- universal color block (defaults = no-op) ----
    vec3 uc = col;
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
    // background = the black void the flock flies in (darkest end)
    uc = mix(uc, bgColor.rgb, bgColor.a * (1.0 - smoothstep(0.0, 0.35, ucL)));

    gl_FragColor = vec4(uc, 1.0);
}

// ---------------------------------------------------------------------------
void main(){
    if (PASSINDEX == 0){
        simPass();
    } else {
        screenPass();
    }
}
