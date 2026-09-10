/*{
  "DESCRIPTION": "3D water ripple field: a persistent wave-equation simulation propagates ripples (audio drops as bass impulses, treble chop, idle drips) and the final pass raymarches the simulated heightfield as reflective/refractive 3D water.",
  "CREDIT": "ShaderClaw3",
  "CATEGORIES": [
    "Generator",
    "3D",
    "Fluid",
    "Audio Reactive"
  ],
  "INPUTS": [
    {
      "NAME": "refrAmt",
      "LABEL": "Refraction",
      "TYPE": "float",
      "DEFAULT": 0.18,
      "MIN": 0,
      "MAX": 0.6
    },
    {
      "NAME": "reflAmt",
      "LABEL": "Reflection",
      "TYPE": "float",
      "DEFAULT": 0.7,
      "MIN": 0,
      "MAX": 1
    },
    {
      "NAME": "texMix",
      "LABEL": "Floor Image",
      "TYPE": "float",
      "DEFAULT": 0,
      "MIN": 0,
      "MAX": 1
    },
    {
      "NAME": "inputImage",
      "TYPE": "image",
      "LABEL": "Texture"
    },
    {
      "NAME": "heightScale",
      "LABEL": "Wave Height",
      "TYPE": "float",
      "DEFAULT": 0.12,
      "MIN": 0,
      "MAX": 0.4,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "waveSpeedC",
      "LABEL": "Wave Speed",
      "TYPE": "float",
      "DEFAULT": 0.3,
      "MIN": 0,
      "MAX": 0.5,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "damping",
      "LABEL": "Ripple Persistence",
      "TYPE": "float",
      "DEFAULT": 0.995,
      "MIN": 0.985,
      "MAX": 0.999,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "dropStrength",
      "LABEL": "Drop Strength",
      "TYPE": "float",
      "DEFAULT": 0.6,
      "MIN": 0,
      "MAX": 2,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "waterTint",
      "LABEL": "Water Tint",
      "TYPE": "color",
      "DEFAULT": [
        0.04,
        0.18,
        0.26,
        1
      ],
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
      "NAME": "camSpin",
      "LABEL": "Camera Spin",
      "TYPE": "float",
      "DEFAULT": 0.08,
      "MIN": 0,
      "MAX": 1,
      "GROUP": "Camera / Layout"
    },
    {
      "NAME": "cameraTilt",
      "LABEL": "Camera Tilt",
      "TYPE": "float",
      "DEFAULT": 0.62,
      "MIN": 0.2,
      "MAX": 1.2,
      "GROUP": "Camera / Layout"
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
      "TARGET": "simBuf",
      "PERSISTENT": true
    },
    {}
  ]
}*/

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
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

// One of three slowly-drifting ripple sources, in uv space (0..1).
vec2 sourcePos(float idx, float t){
    vec2 h = hash21(idx * 13.17 + 1.0);
    // slow lissajous drift so each source wanders
    float a = t * (0.07 + 0.05 * h.x) + h.y * 6.2831;
    float b = t * (0.05 + 0.04 * h.y) + h.x * 6.2831;
    vec2 c = vec2(0.5) + vec2(0.30 * cos(a), 0.30 * sin(b));
    return c;
}

// gaussian bump contribution at uv from a source at sp
float bump(vec2 uv, vec2 sp, float radius2, float amp, vec2 aspect){
    vec2 d = (uv - sp) * aspect;
    float d2 = dot(d, d);
    return amp * exp(-d2 / radius2);
}

// ---------------------------------------------------------------------------
// PASS 0 : wave-equation simulation written into simBuf
// ---------------------------------------------------------------------------
void simPass(){
    vec2 uv = gl_FragCoord.xy / RENDERSIZE;
    vec2 tx = 1.0 / RENDERSIZE;

    float bass   = audioBass * audioReact;
    float mid    = audioMid  * audioReact;
    float treble = audioHigh * audioReact;

    // decode current (r) and previous (g) height
    vec4 c  = texture2D(simBuf, uv);
    float h  = c.r * 2.0 - 1.0;
    float hp = c.g * 2.0 - 1.0;

    // neighbours (current height only)
    float hN = texture2D(simBuf, uv + vec2(0.0,  tx.y)).r * 2.0 - 1.0;
    float hS = texture2D(simBuf, uv + vec2(0.0, -tx.y)).r * 2.0 - 1.0;
    float hE = texture2D(simBuf, uv + vec2( tx.x, 0.0)).r * 2.0 - 1.0;
    float hW = texture2D(simBuf, uv + vec2(-tx.x, 0.0)).r * 2.0 - 1.0;

    // discrete wave equation (Verlet-style, stable for waveSpeedC<=0.5)
    float lap  = (hN + hS + hE + hW - 4.0 * h);
    float cc   = clamp(waveSpeedC, 0.0, 0.5);
    float hNew = (2.0 * h - hp) + cc * cc * lap;
    hNew *= clamp(damping, 0.985, 0.999);

    // aspect so drops stay round regardless of canvas ratio
    vec2 aspect = vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);
    float t = TIME;

    // --- inject drops for variation ---------------------------------------
    // continuous gentle excitation at all three sources so it's alive in silence
    for (int i = 0; i < 3; i++){
        float fi = float(i);
        vec2 sp = sourcePos(fi, t);

        // tiny idle wobble per source (keeps a living shimmer with no audio)
        float idleW = 0.012 * (0.6 + 0.4 * sin(t * (1.3 + fi * 0.7) + fi * 2.0));
        hNew += bump(uv, sp, 0.0009, idleW * dropStrength, aspect);

        // continuous bass-driven feed (small, K<=1.5 style scaling)
        float bassFeed = 0.030 * bass * dropStrength;
        hNew += bump(uv, sp, 0.0012, bassFeed, aspect);
    }

    // strong gated bass splash at one rotating source (positional pulse, K<=0.6)
    if (bass > 0.18){
        float pick = floor(mod(t * 0.9, 3.0));
        vec2 sp = sourcePos(pick, t);
        float amp = (0.18 + 0.45 * bass) * dropStrength;
        hNew += bump(uv, sp, 0.0016, amp, aspect);
    }

    // treble surface chop: a few tiny high-frequency speckle bumps
    if (treble > 0.05){
        for (int j = 0; j < 4; j++){
            float fj = float(j);
            vec2 hp2 = hash21(fj * 7.31 + floor(t * 6.0) * 1.7);
            float spk = bump(uv, hp2, 0.00018,
                             0.020 * treble * dropStrength, aspect);
            hNew += spk;
        }
    }

    // faint idle drip on ~3s cycle so silence still ripples
    float cyc = fract(t / 3.0);
    if (cyc < 0.04){
        vec2 dp = hash21(floor(t / 3.0) * 3.0 + 0.5);
        float dripAmp = (0.10 + 0.10 * sin(t)) * dropStrength;
        hNew += bump(uv, dp, 0.0010, dripAmp, aspect);
    }

    // clamp to keep the 8-bit encoding from saturating / blowing up
    hNew = clamp(hNew, -0.98, 0.98);

    // warmup: flat field for first couple frames
    if (FRAMEINDEX < 2){
        hNew = 0.0;
        h    = 0.0;
    }

    // store: new height in r, previous (this-frame current) height in g
    gl_FragColor = vec4(hNew * 0.5 + 0.5, h * 0.5 + 0.5, 0.0, 1.0);
}

// ---------------------------------------------------------------------------
// SCREEN PASS : raymarch the heightfield as 3D water
// ---------------------------------------------------------------------------

// sample world height at plane-uv (0..1)
float heightAt(vec2 puv){
    puv = clamp(puv, 0.0, 1.0);
    return (texture2D(simBuf, puv).r * 2.0 - 1.0) * heightScale;
}

// world xz in [-1,1] -> plane uv in [0,1]
vec2 worldToUV(vec2 xz){
    return xz * 0.5 + 0.5;
}

// surface normal from height gradient (central differences)
vec3 surfNormal(vec2 puv){
    vec2 tx = 1.0 / RENDERSIZE;
    float hL = heightAt(puv - vec2(tx.x, 0.0));
    float hR = heightAt(puv + vec2(tx.x, 0.0));
    float hD = heightAt(puv - vec2(0.0, tx.y));
    float hU = heightAt(puv + vec2(0.0, tx.y));
    // world step between samples (uv spans 2 world units)
    float dx = 2.0 * tx.x;
    float dz = 2.0 * tx.y;
    vec3 n = normalize(vec3(-(hR - hL) / (2.0 * dx),
                            1.0,
                            -(hU - hD) / (2.0 * dz)));
    return n;
}

// procedural sky / environment
vec3 skyColor(vec3 rd){
    float up = clamp(rd.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 horizon = vec3(0.05, 0.08, 0.13);
    vec3 zenith  = vec3(0.01, 0.02, 0.05);
    vec3 col = mix(horizon, zenith, pow(up, 0.8));
    // sun hotspot
    vec3 sunDir = normalize(vec3(-0.35, 0.55, -0.75));
    float s = max(dot(rd, sunDir), 0.0);
    col += vec3(1.0, 0.85, 0.6) * pow(s, 220.0) * 1.6;       // tight sun disk
    col += vec3(0.5, 0.45, 0.4) * pow(s, 8.0) * 0.18;        // soft glow
    return col;
}

// procedural pool floor (used when no image, or blended)
vec3 proceduralFloor(vec2 fuv, float depth){
    // caustic-ish interference pattern
    vec2 p = fuv * 9.0;
    float c = 0.0;
    c += sin(p.x + TIME * 0.6) * sin(p.y - TIME * 0.4);
    c += sin(p.x * 1.7 - TIME * 0.3) * sin(p.y * 1.3 + TIME * 0.5);
    c = c * 0.25 + 0.5;
    c = pow(clamp(c, 0.0, 1.0), 2.2);
    vec3 deep = waterTint.rgb * 0.5;
    vec3 lit  = waterTint.rgb + vec3(0.15, 0.25, 0.25);
    vec3 col  = mix(deep, lit, c);
    return col;
}

void screenPass(){
    vec2 uv  = gl_FragCoord.xy / RENDERSIZE;
    vec2 res = RENDERSIZE;
    vec2 ndc = (gl_FragCoord.xy - 0.5 * res) / res.y;   // -.. .. , aspect correct

    float bass   = audioBass * audioReact;
    float mid    = audioMid  * audioReact;
    float treble = audioHigh * audioReact;

    // --- camera : tilted, looking across the plane, slow orbit ------------
    float ang = TIME * camSpin * 0.4;
    float orbitR = 2.3;
    vec3 ro = vec3(sin(ang) * orbitR, 1.4 + cameraTilt * 0.6, cos(ang) * orbitR);
    vec3 ta = vec3(0.0, 0.0, 0.0);

    vec3 fwd = normalize(ta - ro);
    vec3 rgt = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 upv = cross(fwd, rgt);
    float fov = 1.2;
    vec3 rd = normalize(fwd + (ndc.x * rgt + ndc.y * upv) * fov);

    // --- raymarch the plane y = H(xz) ------------------------------------
    // only descend toward the plane if looking downward
    vec3 col = skyColor(rd);
    bool hit = false;
    vec3 hitP = vec3(0.0);
    vec2 hitUV = vec2(0.5);

    float tNear = 0.05;
    float tFar  = 8.0;
    float tStep = (tFar - tNear) / 80.0;
    float tPrev = tNear;
    float dPrev = 0.0;

    // initial sign
    {
        vec3 p0 = ro + rd * tNear;
        vec2 u0 = worldToUV(p0.xz);
        dPrev = p0.y - heightAt(u0);
    }

    for (int i = 1; i < 80; i++){
        float t = tNear + tStep * float(i);
        vec3 p = ro + rd * t;
        vec2 puv = worldToUV(p.xz);
        // outside plane bounds -> keep marching, treat as no surface
        float d = p.y - heightAt(puv);

        bool inBounds = (p.xz.x > -1.05 && p.xz.x < 1.05 &&
                         p.xz.y > -1.05 && p.xz.y < 1.05);

        if (inBounds && dPrev > 0.0 && d <= 0.0){
            // crossing between tPrev and t : refine with bisection
            float ta2 = tPrev;
            float tb2 = t;
            for (int k = 0; k < 2; k++){
                float tm = 0.5 * (ta2 + tb2);
                vec3 pm = ro + rd * tm;
                float dm = pm.y - heightAt(worldToUV(pm.xz));
                if (dm > 0.0) ta2 = tm; else tb2 = tm;
            }
            float tf = 0.5 * (ta2 + tb2);
            hitP = ro + rd * tf;
            hitUV = worldToUV(hitP.xz);
            hit = true;
            break;
        }
        tPrev = t;
        dPrev = d;
        if (t > tFar) break;
    }

    if (hit){
        vec3 n = surfNormal(hitUV);
        vec3 viewDir = normalize(ro - hitP);

        // fresnel
        float fres = pow(1.0 - clamp(dot(n, viewDir), 0.0, 1.0), 5.0);
        fres = mix(0.04, 1.0, fres);

        // --- reflection ---
        vec3 refl = reflect(-viewDir, n);
        vec3 reflCol = skyColor(refl) * reflAmt;

        // --- refraction (floor through the surface) ---
        vec2 fuv = hitUV + n.xz * refrAmt;
        vec3 floorCol;
        if (texMix > 0.0){
            vec3 imgCol = texture2D(inputImage, clamp(fuv, 0.0, 1.0)).rgb;
            vec3 procCol = proceduralFloor(fuv, 1.0);
            floorCol = mix(procCol, imgCol, clamp(texMix, 0.0, 1.0));
        } else {
            floorCol = proceduralFloor(fuv, 1.0);
        }
        // depth tint : deeper troughs read darker / more saturated
        float depthT = clamp(0.5 - hitP.y * 1.5, 0.0, 1.0);
        floorCol *= mix(0.55, 1.0, depthT);
        floorCol = mix(floorCol, waterTint.rgb * floorCol, 0.35);

        // compose floor -> sky reflection by fresnel
        col = mix(floorCol, reflCol, clamp(fres, 0.0, 1.0));

        // specular sun glint ; treble sharpens / brightens (K<=0.6)
        vec3 sunDir = normalize(vec3(-0.35, 0.55, -0.75));
        vec3 halfv = normalize(sunDir + viewDir);
        float spePow = mix(120.0, 320.0, clamp(treble, 0.0, 1.0)); // treble sharpen
        float spec = pow(max(dot(n, halfv), 0.0), spePow);
        float speGain = 1.0 + 0.6 * treble;                        // treble brighten K<=0.6
        col += vec3(1.0, 0.92, 0.75) * spec * 1.4 * speGain;

        // mid-driven ambient lift on the water body, plus a bass/punch glow
        // that reads instantly (doesn't wait on ripple propagation through simBuf)
        float punch = audioPunch * audioReact;
        col += waterTint.rgb * (0.05 + 0.55 * mid) * (1.0 + 0.5 * bass);
        col += vec3(1.0, 0.9, 0.75) * punch * 0.16;
    }

    // tonemap + gamma
    col = col / (1.0 + col);
    col = pow(max(col, 0.0), vec3(1.0 / 2.2));

    // Continuous smooth band-follow (ambient fix): beat-less swells only fed
    // the sim, where propagation lag + the field's own churn hid them — so
    // the bands also breathe the whole frame's luminance directly, water and
    // sky alike. Round 3 (measured): the pre-tonemap 0.28/0.17 bass/mid
    // version still scored ambient 0.28 — the tonemap+gamma chain compressed
    // it under the sim's own churn. POST-tonemap it acts on display values
    // directly, and the bass/mid/high weights track the actual band mix of
    // the music (broadband, so no single sinusoid dominates the response).
    // Silence -> exactly 1.0.
    float followA = clamp(audioReact, 0.0, 1.5);
    col *= 1.0 + (0.30 * clamp(audioBass, 0.0, 1.0)
                + 0.19 * clamp(audioMid, 0.0, 1.0)
                + 0.11 * clamp(audioHigh, 0.0, 1.0)) * followA;

    // ---- universal color block (defaults = no-op) ----
    float ucL = dot(col, vec3(0.299, 0.587, 0.114));
    vec3 uc = mix(vec3(ucL), col, colorBoost);
    if (hueShift > 0.0005) {
        float hueA = hueShift * 6.2831853;
        float hueC = cos(hueA), hueSn = sin(hueA);
        mat3 hueM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
                  + hueC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
                  + hueSn * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
        uc = clamp(hueM * uc, 0.0, 1.0);
    }
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
