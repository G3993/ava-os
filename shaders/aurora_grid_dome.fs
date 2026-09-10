/*{
  "DESCRIPTION": "Aurora Grid Dome: a raymarched neon grid floor stretching to a black horizon under a soft flowing aurora dome. The dome's flow-noise field lives in a persistent buffer that is advected each frame by an analytic curl flow (Milkdrop-style feedback) — audio injects energy into the drift, the field's own dynamics carry it. Bass breathes the dome open, mid drives flow speed + hue drift, beats ripple a bright color-shift through the bands, highs twinkle the starfield behind it.",
  "CREDIT": "ShaderClaw3",
  "CATEGORIES": [
    "Generator",
    "3D",
    "Audio Reactive"
  ],
  "INPUTS": [
    {
      "NAME": "auroraGlow",
      "LABEL": "Aurora Glow",
      "TYPE": "float",
      "DEFAULT": 1,
      "MIN": 0,
      "MAX": 2
    },
    {
      "NAME": "gridGlow",
      "LABEL": "Grid Glow",
      "TYPE": "float",
      "DEFAULT": 0.85,
      "MIN": 0,
      "MAX": 2
    },
    {
      "NAME": "texMix",
      "LABEL": "Starfield Image Mix",
      "TYPE": "float",
      "DEFAULT": 0,
      "MIN": 0,
      "MAX": 1
    },
    {
      "NAME": "inputImage",
      "TYPE": "image",
      "LABEL": "Starfield Image"
    },
    {
      "NAME": "domeRadius",
      "LABEL": "Dome Radius",
      "TYPE": "float",
      "DEFAULT": 1.3,
      "MIN": 0.6,
      "MAX": 2.2,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "gridScale",
      "LABEL": "Grid Density",
      "TYPE": "float",
      "DEFAULT": 7,
      "MIN": 2,
      "MAX": 20,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "flowSpeed",
      "LABEL": "Aurora Flow Speed",
      "TYPE": "float",
      "DEFAULT": 0.6,
      "MIN": 0,
      "MAX": 2,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "hueShift",
      "LABEL": "Aurora Hue Shift",
      "TYPE": "float",
      "DEFAULT": 0,
      "MIN": 0,
      "MAX": 1,
      "GROUP": "Color"
    },
    {
      "NAME": "gridTint",
      "LABEL": "Grid Tint",
      "TYPE": "color",
      "DEFAULT": [
        0.1,
        0.55,
        0.85,
        1
      ],
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
      "NAME": "camSpin",
      "LABEL": "Camera Spin",
      "TYPE": "float",
      "DEFAULT": 0.1,
      "MIN": 0,
      "MAX": 1,
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
    },
    {
      "NAME": "domePulseAmt",
      "LABEL": "Bass Dome Breathe",
      "TYPE": "float",
      "DEFAULT": 0.16,
      "MIN": 0,
      "MAX": 0.5,
      "GROUP": "Audio Reactivity"
    },
    {
      "NAME": "pulseTrail",
      "LABEL": "Beat Ripple Trail",
      "TYPE": "float",
      "DEFAULT": 0.965,
      "MIN": 0.9,
      "MAX": 0.99,
      "GROUP": "Audio Reactivity"
    }
  ],
  "PASSES": [
    {
      "TARGET": "auroraBuf",
      "PERSISTENT": true
    },
    {}
  ]
}*/

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
float hash21(float p){
    vec3 p3 = fract(vec3(p) * vec3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float knee(float x, float lo, float hi){ return smoothstep(lo, hi, x); }

// Analytic swirling potential (sum of two travelling sine waves) — its curl
// gives a divergence-free flow field with no noise textures / derivatives.
vec2 curlFlow(vec2 p, float t, float speed){
    float k1x = 2.6; float k1y = 1.7; float w1 = 0.55;
    float k2x = 1.3; float k2y = 3.1; float w2 = 0.37;
    float a1 = 1.0;  float a2 = 0.6;

    float ph1 = w1 * t * speed;
    float ph2b = w1 * t * speed * 0.7;
    float ph2 = w2 * t * speed * 1.3;
    float ph2c = w2 * t * speed * 0.6;

    float s1 = sin(k1x * p.x + ph1);
    float c1 = cos(k1y * p.y - ph2b);
    float s2 = sin(k2x * p.x - ph2);
    float c2 = cos(k2y * p.y + ph2c);

    float dpsidx = a1 * k1x * cos(k1x * p.x + ph1) * c1
                 + a2 * k2x * cos(k2x * p.x - ph2) * c2;
    float dpsidy = -a1 * k1y * s1 * sin(k1y * p.y - ph2b)
                   -a2 * k2y * s2 * sin(k2y * p.y + ph2c);

    return vec2(dpsidy, -dpsidx);
}

// ---------------------------------------------------------------------------
// PASS 0 : persistent aurora flow-noise field, advected by curlFlow
//   r = primary band density   g = hue-phase accumulator (wraps 0..1)
//   b = beat-pulse energy      a = secondary (finer) band density
// ---------------------------------------------------------------------------
void simPass(){
    vec2 uv = gl_FragCoord.xy / RENDERSIZE;

    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6);
    float midP  = pow(knee(audioMid,  0.05, 0.85), 1.3);
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2);
    float drive = 0.25 + 0.75 * knee(audioEnergy, 0.05, 0.9);
    float amt   = clamp(audioReact, 0.0, 2.0);
    float dt    = clamp(TIMEDELTA, 0.0005, 0.06);

    // mid drives flow speed; there is always some drift, even in silence
    float speed = flowSpeed * (0.55 + 1.0 * midP * amt + 0.35 * drive);
    vec2 vel = curlFlow(uv * 3.2, TIME, speed) * 0.05;

    vec2 back = clamp(uv - vel, 0.0, 1.0);
    vec4 prevC = texture2D(auroraBuf, back);

    float D1p = prevC.r;
    float Pp   = prevC.g;
    float Pulp = prevC.b;
    float D2p  = prevC.a;

    // wavy azimuth ribbons -> the aurora-curtain shape, always alive
    float x = uv.x * 6.28318530718;
    float ribbon = sin(x * 3.0 + TIME * 0.32) * 0.5
                 + sin(x * 5.3 - TIME * 0.21) * 0.3
                 + sin(x * 8.7 + TIME * 0.57) * 0.2;
    ribbon = ribbon * 0.5 + 0.5;
    float vband = smoothstep(0.0, 0.30, uv.y) * smoothstep(1.0, 0.55, uv.y);
    float inject1 = ribbon * vband;

    float D1 = D1p * 0.965
             + inject1 * (0.045 + 0.05 * drive)
             + inject1 * 0.10 * bassP * amt;

    float x2 = uv.x * 6.28318530718 * 1.7 + 4.2;
    float ribbon2 = sin(x2 * 2.0 - TIME * 0.44) * 0.5 + sin(x2 * 4.4 + TIME * 0.29) * 0.5;
    ribbon2 = ribbon2 * 0.5 + 0.5;
    float D2 = D2p * 0.955 + ribbon2 * vband * (0.035 + 0.03 * highP * amt);

    // hue drift, mid-driven, always advancing gently at rest (dt tuned around 1/30s)
    float hueSpeed = 0.012 + 0.05 * midP * amt + 0.01 * drive;
    float P = fract(Pp + hueSpeed * dt * 30.0);

    // beat impulse: injected where bands currently sit, then advected + decayed
    // so a hit stays visible rippling through the aurora for seconds, not a flash
    float Pul = Pulp * clamp(pulseTrail, 0.90, 0.99);
    Pul += ribbon * vband * audioBeatPulse * audioBeatPulse * amt * 0.9;

    D1  = clamp(D1, 0.0, 1.0);
    D2  = clamp(D2, 0.0, 1.0);
    Pul = clamp(Pul, 0.0, 1.0);

    if (FRAMEINDEX < 2){
        D1 = 0.25; D2 = 0.15; Pul = 0.0; P = 0.0;
    }

    gl_FragColor = vec4(D1, P, Pul, D2);
}

// ---------------------------------------------------------------------------
// SCREEN PASS : raymarch a glowing grid floor + a volumetric aurora dome
// ---------------------------------------------------------------------------

// hard neon grid floor, plane y = 0, analytic ray/plane intersection
vec3 renderFloor(vec3 ro, vec3 rd, float bassP, float beatP, out bool hitFloor, out float floorDist){
    hitFloor = false;
    floorDist = 0.0;
    vec3 col = vec3(0.0);
    if (rd.y < -0.001){
        float t = -ro.y / rd.y;
        if (t > 0.0){
            vec3 p = ro + rd * t;
            hitFloor = true;
            floorDist = t;

            vec2 gp = fract(p.xz * gridScale) - 0.5;
            float d = min(abs(gp.x), abs(gp.y));
            float line = 1.0 - smoothstep(0.0, 0.03, d);

            vec2 gpM = fract(p.xz * (gridScale * 0.2)) - 0.5;
            float dM = min(abs(gpM.x), abs(gpM.y));
            float lineM = 1.0 - smoothstep(0.0, 0.018, dM);

            float glow = line * 0.55 + lineM * 0.9;
            float fog = exp(-t * 0.10);

            vec3 base = gridTint.rgb * gridGlow;
            col = base * glow * fog;
            col += base * 0.05 * fog;                     // faint idle plane glow, never dead
            col *= (1.0 + 0.55 * bassP + 1.1 * beatP);     // bass breathe + beat flash
        }
    }
    return col;
}

// volumetric raymarch through the aurora shell draped over the dome
vec4 marchAurora(vec3 ro, vec3 rd, float domeR, float thickness){
    vec3 domeCenter = vec3(0.0, 0.0, 0.0);
    float domeBound = domeR + thickness * 2.2;

    vec3 oc = ro - domeCenter;
    float b = dot(oc, rd);
    float c = dot(oc, oc) - domeBound * domeBound;
    float h = b * b - c;

    vec3 accCol = vec3(0.0);
    float accA = 0.0;

    if (h > 0.0){
        h = sqrt(h);
        float t0 = max(-b - h, 0.05);
        float t1 = -b + h;
        if (t1 > t0){
            float steps = 30.0;
            float stepDt = (t1 - t0) / steps;
            for (int i = 0; i < 30; i++){
                float t = t0 + stepDt * (float(i) + 0.5);
                vec3 p = ro + rd * t;
                float above = smoothstep(-0.10, 0.06, p.y);
                if (above > 0.001){
                    float r = length(p - domeCenter);
                    float shell = exp(-pow((r - domeR) / thickness, 2.0));
                    if (shell > 0.003){
                        vec3 dir = normalize(p - domeCenter);
                        float theta = atan(dir.z, dir.x) / 6.28318530718 + 0.5;
                        float phi = clamp(dir.y, 0.0, 1.0);
                        vec4 field = texture2D(auroraBuf, vec2(theta, phi));
                        float dens = field.r * 0.62 + field.a * 0.38;
                        float w = clamp(dens * shell * above, 0.0, 1.0) * stepDt * 1.6;

                        float huePos = fract(field.g + hueShift);
                        vec3 baseCol = audioPalette(huePos);
                        vec3 pulseCol = mix(baseCol, audioPalAccent, clamp(field.b, 0.0, 1.0) * 0.75);
                        vec3 sampleCol = pulseCol * (0.55 + 1.35 * dens) * auroraGlow;

                        float wA = w * (1.0 - accA);
                        accCol += sampleCol * wA;
                        accA += wA;
                        if (accA > 0.985) break;
                    }
                }
            }
        }
    }
    return vec4(accCol, clamp(accA, 0.0, 1.0));
}

void screenPass(){
    vec2 res = RENDERSIZE;
    vec2 ndc = (gl_FragCoord.xy - 0.5 * res) / res.y;

    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6);
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2);
    float amt   = clamp(audioReact, 0.0, 2.0);
    float beatP = audioBeatPulse * audioBeatPulse * amt;

    // --- camera: slow orbit around the dome/floor origin --------------------
    float ang = TIME * camSpin * 0.30;
    float orbitR = 3.4;
    vec3 ro = vec3(sin(ang) * orbitR, 1.15, cos(ang) * orbitR);
    vec3 ta = vec3(0.0, 0.85, 0.0);
    vec3 fwd = normalize(ta - ro);
    vec3 rgt = normalize(cross(vec3(0.0, 1.0, 0.0), fwd));
    vec3 upv = cross(fwd, rgt);
    float fov = 1.15;
    vec3 rd = normalize(fwd + (ndc.x * rgt + ndc.y * upv) * fov);

    // --- background: near-black sky + twinkling starfield / image env -------
    vec2 suv = vec2(atan(rd.z, rd.x) / 6.28318530718 + 0.5, clamp(rd.y * 0.5 + 0.5, 0.0, 1.0));
    vec3 zenith  = vec3(0.0, 0.0, 0.01);
    vec3 horizon = vec3(0.02, 0.02, 0.05);
    vec3 skyBase = mix(horizon, zenith, pow(clamp(rd.y, 0.0, 1.0), 0.6));

    vec2 scell = floor(suv * 240.0);
    float sh = hash21(dot(scell, vec2(12.9898, 78.233)) + 11.0);
    float starMask = step(0.9965, sh);
    float twRate = 5.0 + 16.0 * highP * amt;              // highs speed up twinkle
    float tw = 0.5 + 0.5 * sin(TIME * twRate + sh * 71.0);
    vec3 stars = vec3(0.75, 0.82, 1.0) * starMask * (0.35 + 0.65 * tw);

    vec3 bgProc = skyBase + stars;
    vec3 bg = bgProc;
    if (texMix > 0.0){
        vec3 imgCol = texture2D(inputImage, clamp(suv, 0.0, 1.0)).rgb;
        bg = mix(bgProc, bgProc * 0.35 + imgCol * 0.9, clamp(texMix, 0.0, 1.0));
    }
    // User background: blend the sky region toward the chosen color.
    bg = mix(bg, bgColor.rgb, bgColor.a);

    // --- floor ---------------------------------------------------------------
    bool hitFloor;
    float floorDist;
    vec3 floorCol = renderFloor(ro, rd, bassP, beatP, hitFloor, floorDist);
    vec3 col = bg;
    if (hitFloor) col = floorCol;

    // --- aurora dome -----------------------------------------------------------
    float domeR = domeRadius * (1.0 + domePulseAmt * bassP * amt); // bass breathes it open
    float thickness = 0.55;
    vec4 aurora = marchAurora(ro, rd, domeR, thickness);
    col = mix(col, aurora.rgb, clamp(aurora.a, 0.0, 1.0));

    // house look: fade to black at the frame edges
    float vig = smoothstep(1.5, 0.25, length(ndc));
    col *= mix(0.15, 1.0, vig);

    // tonemap + gamma
    col = col / (1.0 + col);
    col = pow(max(col, 0.0), vec3(1.0 / 2.2));

    // ---- universal color block (defaults = no-op; hueShift already native) ----
    float ucL = dot(col, vec3(0.299, 0.587, 0.114));
    col = mix(vec3(ucL), col, colorBoost);                    // saturation

    gl_FragColor = vec4(col, 1.0);
}

// ---------------------------------------------------------------------------
void main(){
    if (PASSINDEX == 0){
        simPass();
    } else {
        screenPass();
    }
}
