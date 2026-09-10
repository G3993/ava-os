/*{
  "DESCRIPTION": "Moving Around — six colored point lights roam a 2D SDF room (dot grid, pillars, spinning bar, glowing ring) casting real-time soft shadows via a 1D radial shadow map pass. Bass flashes the center light and swings the rotor, mids drive the roaming lights, highs flicker the side bobbers.",
  "CREDIT": "1D radial shadow-map technique from Shadertoy, ShaderClaw audio port",
  "CATEGORIES": [
    "Generator"
  ],
  "INPUTS": [
    {
      "NAME": "speed",
      "LABEL": "Speed",
      "TYPE": "float",
      "DEFAULT": 1.0,
      "MIN": 0.1,
      "MAX": 3.0
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
      "NAME": "brightness",
      "LABEL": "Brightness",
      "TYPE": "float",
      "DEFAULT": 1.0,
      "MIN": 0.2,
      "MAX": 3.0
    },
    {
      "NAME": "tintColor",
      "LABEL": "Tint",
      "TYPE": "color",
      "GROUP": "Color",
      "DEFAULT": [1.0, 1.0, 1.0, 1.0]
    }
  ],
  "PASSES": [
    {
      "TARGET": "shadowMap"
    },
    {}
  ]
}*/

#define NUM_LIGHTS 6
#define MAX_STEPS 48
#define TAU 6.283185307179586
#define WALL_COLOR vec3(1.0, 0.5, 0.1)
#define FLOOR_COLOR vec3(0.4, 0.4, 0.4)
#define AMBIENT_LIGHT vec3(0.1, 0.1, 0.1)
#define DIST_MAX 2.2

float gMT;    // motion clock
float gBass, gMid, gHigh, gLevel;
vec2 gRes;    // y-normalized resolution

float knee(float x, float lo, float hi) { return smoothstep(lo, hi, x); }

// Shapes
float sdCircle(float r, vec2 uv) { return length(uv) - r; }
float sdRing(float ir, float outr, vec2 uv) {
    return abs(length(uv) - (ir+outr)/2.0) - (outr - ir);
}
float sdBox(float s, vec2 uv) { return max(abs(uv.x), abs(uv.y)) - s; }
float sdRect(vec2 s, vec2 uv) {
    uv = abs(uv) - s;
    return max(uv.x, uv.y);
}

// Operations
float opU(float a, float b) { return min(a, b); }
float opS(float a, float b) { return max(-a, b); }

// Domain modifiers
mat2 Rotate(float a) { return mat2(cos(a), sin(a), -sin(a), cos(a)); }
vec2 Rep1(vec2 uv, float r) { uv.x = mod(uv.x, r) - r/2.0; return uv; }
vec2 Rep2(vec2 uv, vec2 r) { return mod(uv, r) - r/2.0; }

// Scene — must evaluate identically in both passes
float Scene(vec2 uv) {
    float d = -sdRect(gRes/2.0 - 0.05, uv);
    vec2 rp = Rep2(uv, vec2(0.2));
    d = opU(sdCircle(0.02, rp), d);
    rp = Rep1(uv, 0.2);
    d = opU(sdRect(vec2(0.005,0.1), rp), d);
    d = opS(sdBox(0.2, uv), d);
    d = opU(sdRing(0.08, 0.09, uv), d);
    // rotor: bass gives the spin a smoothed extra swing
    d = opS(sdRect(vec2(0.11,0.03), uv * Rotate(gMT + 0.2 * audioReact * gBass)), d);
    return d;
}

vec2 lightOrigin(int id) {
    if (id == 0) return vec2(0.0);
    if (id == 1) return vec2(cos(gMT*0.37), sin(gMT*0.23)) * 0.32;
    if (id == 2) return -vec2(cos(gMT*0.29), sin(gMT*0.41)) * 0.32;
    if (id == 3) {
        float a = -gMT * 0.3;
        return vec2(cos(a), sin(a)) * (0.2 + 0.04 * audioReact * gBass);
    }
    if (id == 4) return vec2( 0.4, sin(3.0*gMT - TAU/4.0) * 0.2);
    return vec2(-0.4, sin(3.0*gMT) * 0.2);
}

vec3 lightColor(int id) {
    float ar = audioReact;
    if (id == 0) return vec3(1.0, 0.2, 0.2) * (sin(gMT*4.0)*2.0 + 2.0)
                        * mix(1.0, 0.35 + 2.4*gBass, ar);
    if (id == 1) return vec3(1.0, 0.6, 0.6) * 2.0 * mix(1.0, 0.4 + 1.8*gMid, ar);
    if (id == 2) return vec3(0.6, 1.0, 0.6) * 2.0 * mix(1.0, 0.4 + 1.8*gMid, ar);
    if (id == 3) return vec3(0.4, 0.4, 1.0) * 4.0 * mix(1.0, 0.4 + 1.6*gBass, ar);
    if (id == 4) return vec3(1.0, 1.0, 0.4) * mix(1.0, 0.3 + 2.2*gHigh, ar);
    return vec3(1.0, 0.4, 1.0) * mix(1.0, 0.3 + 2.2*gHigh, ar);
}

// Pass 0 — radial shadow map. Each light owns a horizontal x-band (width/6),
// written identically down every row, so decoding never depends on which way
// the host orients its render targets. Distance packed 16-bit in rg.
vec4 passShadow() {
    float fx = gl_FragCoord.x * float(NUM_LIGHTS) / RENDERSIZE.x;
    int id = int(fx);
    id = int(min(float(id), float(NUM_LIGHTS - 1)));
    float a = fract(fx) * TAU;
    vec2 dir = vec2(cos(a), sin(a));
    vec2 orig = lightOrigin(id);
    float d = 0.0;
    for (int i = 0; i < MAX_STEPS; i++) {
        float ds = Scene(orig + dir * d);
        d += ds;
        if (ds < 1e-4) break;
    }
    float e = clamp(d / DIST_MAX, 0.0, 1.0) * 255.0;
    return vec4(floor(e)/255.0, fract(e), 0.0, 1.0);
}

float sampleShadow(int id, vec2 rel) {
    float a = fract(atan(rel.y, rel.x)/TAU + 0.5);
    // band-local coordinate, snapped to texel centers: hi/lo packed
    // channels must not be filtered
    float bandW = RENDERSIZE.x / float(NUM_LIGHTS);
    float x = (float(id) + clamp(a, 0.001, 0.999)) * bandW;
    x = (floor(x) + 0.5) / RENDERSIZE.x;
    vec4 sm = texture2D(shadowMap, vec2(x, 0.5));
    float s = (sm.r + sm.g/255.0) * DIST_MAX;
    return 1.0 - smoothstep(s, s + 0.02, length(rel));
}

vec3 mixLights(vec2 uv) {
    vec3 b = AMBIENT_LIGHT * mix(1.0, 0.6 + 1.2*gLevel, audioReact);
    for (int i = 0; i < NUM_LIGHTS; i++) {
        vec2 rel = uv - lightOrigin(i);
        float l = 0.01 / pow(length(vec3(rel, 0.1)), 2.0);
        l *= sampleShadow(i, rel);
        b += lightColor(i) * l;
    }
    return b;
}

vec4 passImage() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE.y - gRes/2.0;
    float psz = 1.0 / RENDERSIZE.y;
    float d = Scene(uv);
    vec3 col = mix(FLOOR_COLOR, WALL_COLOR, smoothstep(psz, 0.0, d));
    col *= mixLights(uv) * brightness * tintColor.rgb;
    return vec4(col, 1.0);
}

void main() {
    gRes = RENDERSIZE.xy / RENDERSIZE.y;
    gMT = TIME * speed;
    gBass  = pow(knee(audioBass, 0.05, 0.85), 1.6);
    gMid   = pow(knee(audioMid,  0.08, 0.90), 1.3);
    gHigh  = pow(knee(audioHigh, 0.10, 0.90), 1.2);
    gLevel = knee(audioLevel, 0.05, 0.90);
    if (PASSINDEX == 0) gl_FragColor = passShadow();
    else                gl_FragColor = passImage();
}
