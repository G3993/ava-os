/*{
  "DESCRIPTION": "Dripping — the input image melts: a compact energy-exchange fluid drags a location field downward so the picture drips and smears like wet paint, relit with a moving specular sheen and a diagonal lens-flair bloom. Bass pulls the drips harder, highs flare the bloom; the melt re-forms on a loop.",
  "CREDIT": "Energy-exchange fluid + location-field warp technique (wyatt-style, Shadertoy), ShaderClaw audio port",
  "CATEGORIES": [
    "Effect"
  ],
  "INPUTS": [
    {
      "NAME": "inputTex",
      "LABEL": "Source",
      "TYPE": "image"
    },
    {
      "NAME": "audioReact",
      "LABEL": "Audio React",
      "TYPE": "float",
      "GROUP": "Audio Reactivity",
      "DEFAULT": 0.5,
      "MIN": 0.0,
      "MAX": 1.0
    },
    {
      "NAME": "loopTime",
      "LABEL": "Melt Loop (s)",
      "TYPE": "float",
      "DEFAULT": 14.0,
      "MIN": 4.0,
      "MAX": 40.0
    },
    {
      "NAME": "flareAmt",
      "LABEL": "Flare",
      "TYPE": "float",
      "DEFAULT": 1.0,
      "MIN": 0.0,
      "MAX": 3.0
    },
    {
      "NAME": "tintColor",
      "LABEL": "Tint",
      "TYPE": "color",
      "GROUP": "Color",
      "DEFAULT": [1.0, 1.0, 1.0, 1.0]
    },
    {
      "NAME": "brightness",
      "LABEL": "Brightness",
      "TYPE": "float",
      "GROUP": "Color",
      "DEFAULT": 1.0,
      "MIN": 0.2,
      "MAX": 3.0
    },
    {
      "NAME": "meltAmt",
      "LABEL": "Melt Strength",
      "TYPE": "float",
      "DEFAULT": 1.0,
      "MIN": 0.2,
      "MAX": 3.0
    }
  ],
  "PASSES": [
    {
      "TARGET": "fluidA",
      "PERSISTENT": true
    },
    {
      "TARGET": "locBuf",
      "PERSISTENT": true
    },
    {
      "TARGET": "litBuf"
    },
    {}
  ]
}*/

float knee(float x, float lo, float hi) { return smoothstep(lo, hi, x); }

// fluid state encode: signed values round-trip the 8-bit buffer
vec4 encA(vec4 v) { return clamp(v / 4.0 + 0.5, 0.0, 1.0); }
vec4 decA(vec4 e) { return (e - 0.5) * 4.0; }
// location field: normalized coords packed 16-bit per axis
vec4 encLoc(vec2 p) {
    p = clamp(p, 0.0, 1.0);
    vec2 e = p * 255.0;
    return vec4(floor(e.x) / 255.0, fract(e.x), floor(e.y) / 255.0, fract(e.y));
}
vec2 decLoc(vec4 t) { return vec2(t.r + t.g / 255.0, t.b + t.a / 255.0); }

// source with a procedural fallback so the effect is alive with no input bound
vec4 srcAt(vec2 loc) {
    if (IMG_SIZE(inputTex).x > 0.5) return IMG_NORM_PIXEL(inputTex, loc);
    vec2 p = loc * 12.0;
    vec3 c = 0.45 + 0.3 * cos(vec3(0.0, 2.1, 4.2) + p.x + sin(p.y * 1.3)
                              + floor(p.y) * 1.7);
    float grid = step(0.08, fract(p.x)) * step(0.08, fract(p.y));
    float check = mod(floor(p.x) + floor(p.y), 2.0);
    c *= 0.55 + 0.45 * check;
    return vec4(c * (0.18 + 0.55 * grid), 1.0);
}

vec4 TA(vec2 U) { return decA(texture2D(fluidA, U / RENDERSIZE.xy)); }
vec2 vel(vec4 b) { return vec2(b.x - b.y, b.z - b.w); }
float pres(vec4 b) { return 0.25 * (b.x + b.y + b.z + b.w); }
vec4 advA(vec2 U) {
    U -= 0.5 * vel(TA(U));
    U -= 0.5 * vel(TA(U));
    return TA(U);
}
vec2 locAt(vec2 U) { return decLoc(texture2D(locBuf, U / RENDERSIZE.xy)); }
vec2 advLoc(vec2 U) {
    U -= 0.5 * vel(TA(U));
    U -= 0.5 * vel(TA(U));
    return locAt(U);
}

bool resetPulse() {
    return FRAMEINDEX < 2 || mod(TIME, max(loopTime, 4.0)) < max(TIMEDELTA, 0.034);
}

// pass 0 — energy-exchange fluid
vec4 passFluid() {
    vec2 U = gl_FragCoord.xy;
    vec2 R = RENDERSIZE.xy;
    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6);

    vec4 Q = advA(U);
    vec4 n = advA(U + vec2(0, 1)), e = advA(U + vec2(1, 0)),
         s = advA(U - vec2(0, 1)), w = advA(U - vec2(1, 0));
    float px = 0.25 * (pres(e) - pres(w));
    float py = 0.25 * (pres(n) - pres(s));
    Q += 0.25 * (n.w + e.y + s.z + w.x) - pres(Q) - vec4(px, -px, py, -py);

    // darker picture areas melt faster; z stays positive so drips always
    // fall the same way (a signed z churned instead of dripping)
    float lum = dot(texture2D(litBuf, U / R).xyz, vec3(0.333));
    float z = clamp(0.9 - lum, 0.08, 1.0);
    Q = mix(mix(Q, 0.25 * (n + e + s + w), 0.01), vec4(pres(Q)), 0.01 * (1.0 - z));
    Q.zw -= 0.001 * meltAmt * z * vec2(1, -1)
          * (1.0 + 2.2 * audioReact * bassP);

    if (resetPulse()) Q = vec4(0.2);
    if (U.x < 3.0 || R.x - U.x < 3.0 || U.y < 3.0 || R.y - U.y < 3.0) Q = vec4(pres(Q));
    return encA(Q);
}

// pass 1 — location field advected by the fluid
vec4 passLoc() {
    vec2 U = gl_FragCoord.xy;
    vec2 Q = advLoc(U);

    vec4 q = TA(U), n = TA(U + vec2(0, 1)), e = TA(U + vec2(1, 0)),
         s = TA(U - vec2(0, 1)), w = TA(U - vec2(1, 0));
    vec2 N = advLoc(U + vec2(0, 1)), E = advLoc(U + vec2(1, 0)),
         S = advLoc(U - vec2(0, 1)), W = advLoc(U - vec2(1, 0));
    Q += 0.25 * ((n.w - q.z) * (N - Q) + (e.y - q.x) * (E - Q)
               + (s.z - q.w) * (S - Q) + (w.x - q.y) * (W - Q));

    if (resetPulse()) Q = U / RENDERSIZE.xy;
    return encLoc(Q);
}

// pass 2 — look the picture up through the warped locations, then relight
vec4 passLit() {
    vec2 U = gl_FragCoord.xy;
    vec2 R = RENDERSIZE.xy;
    vec4 img = srcAt(locAt(U));
    float n = length(srcAt(locAt(U + vec2(0, 1))));
    float e = length(srcAt(locAt(U + vec2(1, 0))));
    float s = length(srcAt(locAt(U - vec2(0, 1))));
    float w = length(srcAt(locAt(U - vec2(1, 0))));
    vec3 no = normalize(vec3(e - w, n - s, 0.35));
    float d = dot(reflect(no, vec3(0, 0, 1)), normalize(vec3(1.0)));
    float sheen = exp(-2.5 * d * d);
    vec3 col = img.rgb * (0.45 + 0.55 * sheen) + vec3(0.18) * pow(sheen, 3.0);
    return vec4(col, 1.0);
}

// pass 3 — diagonal lens-flair bloom composite
vec4 flareTap(vec2 U, vec2 r) {
    vec4 t = texture2D(litBuf, (U + r) / RENDERSIZE.xy);
    return exp(-0.01 * dot(r, r)) * (exp(2.0 * t) - 1.0);
}

vec4 passFinal() {
    vec2 U = gl_FragCoord.xy;
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2);
    vec4 Q = vec4(0.0);
    for (float i = 0.0; i < 7.0; i += 1.1) {
        Q += flareTap(U, vec2(-i, i));
        Q += flareTap(U, vec2(i, i));
        Q += flareTap(U, -vec2(-i, i));
        Q += flareTap(U, -vec2(i, i));
    }
    float flareGain = flareAmt * (1.0 + 2.0 * audioReact * highP);
    Q = texture2D(litBuf, U / RENDERSIZE.xy) * 0.9 + 1e-5 * flareGain * Q;
    Q.rgb *= tintColor.rgb * brightness;
    Q = atan(Q);
    Q.a = 1.0;
    return Q;
}

void main() {
    if      (PASSINDEX == 0) gl_FragColor = passFluid();
    else if (PASSINDEX == 1) gl_FragColor = passLoc();
    else if (PASSINDEX == 2) gl_FragColor = passLit();
    else                     gl_FragColor = passFinal();
}
