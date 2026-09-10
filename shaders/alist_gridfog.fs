/*{
  "DESCRIPTION": "Grid Fog — molten translucent neon fog draped over a dark technical blueprint plate. The milky fog body flows on the bass time-clock, an inner pink-to-green spectrum drifts with spectral brightness, and drips advance down the plate on bar-phase ramps. Layers: grid plate / fog body / inner spectrum / drips. No text.",
  "CREDIT": "ShaderClaw3 A-List VFX",
  "CATEGORIES": [
    "Generator",
    "3D",
    "Abstract",
    "Audio Reactive"
  ],
  "INPUTS": [
    {
      "NAME": "fogAmount",
      "LABEL": "Fog Body",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 1
    },
    {
      "NAME": "dripAmount",
      "LABEL": "Drips",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "flowSpeed",
      "LABEL": "Flow Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 3,
      "DEFAULT": 1,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "spectrumGlow",
      "LABEL": "Inner Spectrum",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 1,
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
      "NAME": "gridAmount",
      "LABEL": "Grid Plate",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1.5,
      "DEFAULT": 1,
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
      "NAME": "audioReactivity",
      "LABEL": "Sound Reactivity",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 1,
      "GROUP": "Audio Reactivity"
    }
  ]
}*/

float h21(vec2 p){ p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }

float vnoise(vec2 p){
  vec2 i = floor(p), f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = h21(i), b = h21(i + vec2(1.0, 0.0));
  float c = h21(i + vec2(0.0, 1.0)), d = h21(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p){
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; i++){ v += a * vnoise(p); p = p * 2.03 + vec2(17.3, 9.1); a *= 0.5; }
  return v;
}

vec3 hsv2rgb(float h, float s, float v){
  vec3 p = abs(fract(vec3(h) + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
  return v * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), s);
}

void main(){
  vec2 uv = gl_FragCoord.xy / RENDERSIZE.xy;
  float aspect = RENDERSIZE.x / RENDERSIZE.y;
  vec2 p = (uv - 0.5) * vec2(aspect, 1.0);

  float react = audioReactivity;
  float px = 1.0 / RENDERSIZE.y; // 1 pixel in uv units — the grid stays razor-sharp at any res

  // ---- clocks: integrated Time uniforms are the motion workhorse (never raw->position)
  float fogT  = TIME * 0.11 * flowSpeed + (audioBassTime * 0.30 + audioMidTime * 0.22) * react;
  float specT = TIME * 0.07 * flowSpeed + audioMidTime  * 0.30 * react;

  // =========================== LAYER 1 — grid plate ===========================
  // dark blueprint plate with fine cyan drafting lines + a slow scan sweep
  vec3 col = vec3(0.009, 0.012, 0.022); // plate base
  // crisp 1-2px plate border (was an ~8px wash)
  float plate = smoothstep(0.030, 0.030 + 2.0 * px, uv.x) * smoothstep(0.970, 0.970 - 2.0 * px, uv.x)
              * smoothstep(0.030, 0.030 + 2.0 * px, uv.y) * smoothstep(0.970, 0.970 - 2.0 * px, uv.y);

  vec2 gq = p * 6.0;
  vec2 g1 = abs(fract(gq) - 0.5);
  vec2 g2 = abs(fract(gq * 4.0) - 0.5);
  float d1 = min(g1.x, g1.y) / 6.0;   // uv distance to nearest coarse line
  float d2 = min(g2.x, g2.y) / 24.0;  // uv distance to nearest fine line
  float coarse = smoothstep(1.6 * px, 0.5 * px, d1);  // razor 1px drafting lines
  float fine   = smoothstep(1.0 * px, 0.3 * px, d2);
  // sparse "circuit" accents: some coarse cells get a brighter frame
  float cellA  = step(0.82, h21(floor(gq) + 3.1));
  float scan   = smoothstep(0.10, 0.0, abs(uv.y - fract(TIME * 0.05 + audioTime * 0.02 * react))); // roaming scanline
  vec3 gridCol = vec3(0.22, 0.52, 0.78);
  float gridLit = 0.32 + react * (0.10 * audioMid + 0.06 * audioMidPresence + 0.10 * audioBassHit);
  col += plate * gridAmount * gridCol * (coarse * (gridLit + 0.34 * cellA) + fine * 0.07 + coarse * scan * 0.22);
  // crisp drafting border rulings framing the plate (1px, blueprint sheet edge)
  float bLine = smoothstep(1.5 * px, 0.5 * px, abs(uv.x - 0.036))
              + smoothstep(1.5 * px, 0.5 * px, abs(uv.x - 0.964))
              + smoothstep(1.5 * px, 0.5 * px, abs(uv.y - 0.036))
              + smoothstep(1.5 * px, 0.5 * px, abs(uv.y - 0.964));
  col += gridAmount * mix(gridCol, vec3(0.80, 0.95, 1.00), 0.40) * clamp(bLine, 0.0, 1.0) * 0.75 * plate;
  col *= 0.10 + 0.90 * plate; // black surround

  // =========================== LAYER 2 — fog body =============================
  // draped molten body: domain-warped fbm sheet with torn edges, flowing downward
  vec2 q = p * vec2(1.35, 1.05);
  vec2 warp = vec2(fbm(q * 1.6 + vec2(0.0, -fogT * 0.6)),
                   fbm(q * 1.6 + vec2(5.2,  fogT * 0.45)));
  float f = fbm(q * 2.1 + (warp - 0.5) * 1.9 + vec2(0.0, -fogT));

  // torn vertical drape mask (fog hangs over the plate like slime)
  float edgeN = fbm(vec2(p.y * 2.4 + fogT * 0.18, 3.7));
  float xw = 0.60 + 0.22 * edgeN;
  float drape = smoothstep(xw, xw - 0.34, abs(p.x - 0.06 * sin(p.y * 2.3 + fogT * 0.5)));
  drape *= smoothstep(0.62, 0.40, abs(p.y)); // fade top/bottom

  float dens = drape * (0.30 + 0.95 * f);
  // LINEAR body swell: direct bands (fast path) + presence (slow set-and-hold)
  float breath = 0.78 + react * (0.19 * audioBass + 0.30 * audioMid + 0.08 * audioBassPresence);
  float alpha = smoothstep(0.28, 0.78, dens * breath) * fogAmount;

  // cheap 3D relief: two offset field taps give a light gradient
  float fL = fbm(q * 2.1 + (warp - 0.5) * 1.9 + vec2(-0.09, -fogT + 0.07));
  float shade = clamp(0.80 + 1.5 * (fL - f), 0.40, 1.28);
  vec3 milk = vec3(0.88, 0.91, 0.95) * shade;
  milk += vec3(0.10) * pow(clamp((f - fL) * 6.0, 0.0, 1.0), 2.0); // satin highlight

  // shadow the plate under thick fog, then lay the milk down
  col *= 1.0 - 0.78 * alpha;
  col = mix(col, milk, alpha * 0.94);
  // razor grid ghosting through the translucent fog body: the blueprint lines
  // read as crisp darker traces under the milk (graph paper under vellum) —
  // lines stay 1px sharp while the milk interior stays soft
  float through = coarse * 0.90 + fine * 0.36;
  col = mix(col, gridCol * 0.24, through * alpha * gridAmount * plate);
  // defined torn-boundary rim: a bright ~2px contour where the fog tears off
  // the plate (interior stays milky-soft — only the boundary gets the edge)
  float bnd = dens * breath;
  float fogRim = smoothstep(0.030, 0.008, abs(bnd - 0.30)) * fogAmount;
  col = mix(col, milk * 1.10, fogRim * 0.45);

  // ========================= LAYER 3 — inner spectrum ==========================
  // molten neon glowing through the thick middle: pink -> red -> yellow -> green
  vec2 dg = vec2(dot(p, vec2(0.80, 0.60)), dot(p, vec2(-0.60, 0.80)));
  float g = fbm(vec2(dg.x * 1.1 - specT * 0.8, dg.y * 3.4 + 2.0));
  float hue = fract(0.86 + g * 0.52 + 0.10 * audioBrightness); // brightness drifts the rainbow
  vec3 neon = hsv2rgb(hue, 0.92, 1.0);
  float glow = spectrumGlow * pow(alpha, 1.5) * smoothstep(0.32, 0.72, g)
             * (0.52 + react * (0.45 * audioMid + 0.25 * audioMidPresence))
             * (1.0 + 0.55 * react * audioBassHit); // kick flash through the milk (eased decay)
  col += neon * glow * 1.00;

  // ============================ LAYER 4 — drips ================================
  // fog drips crawl down the plate, advancing on the bar-phase ramp
  float colId = floor((p.x / aspect + 0.5) * 22.0);
  float hc = h21(vec2(colId, 7.0));
  float hc2 = h21(vec2(colId, 41.0));
  float dripOn = step(hc, 0.60);
  float prog = fract(audioBarPhase + TIME * 0.05 * flowSpeed + hc * 7.0);
  float len = (0.10 + 0.30 * hc2) * smoothstep(0.0, 0.45, prog);
  float tipFade = 1.0 - smoothstep(0.72, 1.0, prog); // ease out before the ramp wraps
  float cx = ((colId + 0.5) / 22.0 - 0.5) * aspect;
  float fogBottom = -0.30 - 0.16 * fbm(vec2(cx * 3.0, 2.2));
  float yTip = fogBottom - len;
  float w = 0.010 + 0.012 * hc2;
  float dx = abs(p.x - cx);
  float inBand = smoothstep(yTip - 0.01, yTip + 0.04, p.y) * (1.0 - smoothstep(fogBottom - 0.02, fogBottom + 0.03, p.y));
  float rTip = length(p - vec2(cx, yTip));
  float drop  = smoothstep(w * 2.0, w * 2.0 - 1.5 * px, rTip);       // crisp droplet at the tip
  float dCore = smoothstep(w, w - 1.5 * px, dx) * inBand;            // crisp drip column
  float drip = dripOn * tipFade * max(dCore, drop) * plate;
  // crisp 1px outline hugging the drip silhouette
  float dEdge = max(smoothstep(1.4 * px, 0.3 * px, abs(dx - w)) * inBand,
                    smoothstep(1.4 * px, 0.3 * px, abs(rTip - w * 2.0)));
  dEdge *= dripOn * tipFade * plate;
  col = mix(col, milk * 1.00, drip * dripAmount * 0.9);
  col = mix(col, milk * 1.06, dEdge * dripAmount * 0.70);
  col += neon * (drip + 0.5 * dEdge) * dripAmount * glow * 0.4; // tips catch the inner light

  // ---- whole-frame LINEAR follower (chop-safe, silent = exact base look)
  col *= 1.0 + react * (0.14 * audioBass + 0.24 * audioMid + 0.08 * audioHigh + 0.12 * audioEnergy);

  // ---- universal color block (defaults = no-op)
  float ucL = dot(col, vec3(0.299, 0.587, 0.114));
  col = mix(vec3(ucL), col, colorBoost);
  if (hueShift > 0.0005){
    float hA = hueShift * 6.2831853;
    float hC = cos(hA), hS = sin(hA);
    mat3 hM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
            + hC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
            + hS * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
    col = clamp(hM * col, 0.0, 1.5);
  }
  // background = the dark surround + un-fogged plate void
  float bgMask = (1.0 - plate) + plate * (1.0 - alpha) * (1.0 - smoothstep(0.0, 0.30, ucL));
  col = mix(col, bgColor.rgb, bgColor.a * clamp(bgMask, 0.0, 1.0));

  gl_FragColor = vec4(col, 1.0);
}
