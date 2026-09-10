/*{
  "CATEGORIES": ["Generator", "Audio Reactive", "Minimal"],
  "DESCRIPTION": "Murmuration — a starling flock in monochrome. Hundreds of tiny white birds stream along a shared wind field around a wandering swarm center; bass tightens and swells the flock, beats bank the whole formation, highs flicker sparse wingbeats. Black sky, white birds, nothing else.",
  "INPUTS": [
    {"NAME": "flockSize",  "LABEL": "Flock Size",   "TYPE": "float", "MIN": 0.2, "MAX": 1.0, "DEFAULT": 0.55},
    {"NAME": "birdCount",  "LABEL": "Bird Density", "TYPE": "float", "MIN": 0.3, "MAX": 1.0, "DEFAULT": 0.75},
    {"NAME": "windSpeed",  "LABEL": "Wind Speed",   "TYPE": "float", "MIN": 0.1, "MAX": 2.0, "DEFAULT": 0.7},
    {"NAME": "birdScale",  "LABEL": "Bird Scale",   "TYPE": "float", "MIN": 0.5, "MAX": 2.0, "DEFAULT": 1.0},
    {"NAME": "ghosting",   "LABEL": "Wing Trails",  "TYPE": "float", "MIN": 0.0, "MAX": 1.0, "DEFAULT": 0.45},
    {"NAME": "skyGlow",    "LABEL": "Sky Glow",     "TYPE": "float", "MIN": 0.0, "MAX": 0.3, "DEFAULT": 0.08},
    {"NAME": "invert",     "LABEL": "Invert",       "TYPE": "float", "MIN": 0.0, "MAX": 1.0, "DEFAULT": 0.0}
  ]
}*/

float hash21(vec2 p) {
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

vec2 hash22(vec2 p) {
    float n = hash21(p);
    return vec2(n, hash21(p + n + 17.31));
}

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash21(i), hash21(i + vec2(1.0, 0.0)), u.x),
               mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), u.x), u.y);
}

float knee(float x, float lo, float hi) { return clamp(smoothstep(lo, hi, x), 0.0, 1.0); }

// Distance to a small oriented dash (a bird = body + blurred wing stroke).
float birdDash(vec2 d, vec2 dir, float len, float wid) {
    float along = dot(d, dir);
    float side  = dot(d, vec2(-dir.y, dir.x));
    float t = clamp(along / max(len, 1e-4), -1.0, 1.0);
    vec2 near = dir * t * len;
    float dist = length(d - near);
    return smoothstep(wid, wid * 0.25, dist - abs(side) * 0.15);
}

void main() {
    vec2 p = (gl_FragCoord.xy - 0.5 * RENDERSIZE) / min(RENDERSIZE.x, RENDERSIZE.y);

    float bassP = pow(knee(audioBass, 0.05, 0.85), 1.6);
    float midP  = pow(knee(audioMid,  0.08, 0.88), 1.3);
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2);
    float drive = 0.25 + 0.75 * knee(audioEnergy, 0.05, 0.9);
    float levelP = knee(audioLevel, 0.03, 0.8);
    float beat  = clamp(audioBeatPulse, 0.0, 1.0);

    // Bounded phase offset, never TIME*drive (whole-flock teleport hazard).
    float mt = TIME * windSpeed * 0.8 + drive * 2.5;

    // Swarm center wanders a lissajous sky path; the whole flock follows it.
    vec2 center = vec2(0.44 * sin(mt * 0.23) + 0.18 * sin(mt * 0.071 + 2.1),
                       0.30 * cos(mt * 0.181) + 0.14 * cos(mt * 0.053));

    // Beat banks the formation: an eased momentary rotation of the wind.
    float bank = 0.5 * beat * beat * (hash21(vec2(floor(TIME * 0.5), 7.0)) > 0.5 ? 1.0 : -1.0);

    // Flock radius: bass gathers the birds (murmurations contract on impact);
    // sheer loudness swells the cloud so quiet passages visibly thin it out.
    float spread = flockSize * (0.70 - 0.28 * bassP + 0.10 * midP + 0.35 * levelP);

    float v = 0.0;

    // Two interleaved grids = two depth shells of birds.
    for (int layer = 0; layer < 2; layer++) {
        float fl   = float(layer);
        float cell = mix(0.055, 0.085, fl) / max(birdCount, 0.05);
        float depth = mix(1.0, 0.55, fl);          // far shell dimmer, smaller

        // 3x3 neighborhood so dashes cross cell borders cleanly.
        vec2 gid = floor(p / cell);
        for (int oy = -1; oy <= 1; oy++)
        for (int ox = -1; ox <= 1; ox++) {
            vec2 id = gid + vec2(float(ox), float(oy));
            vec2 rnd = hash22(id + fl * 91.7);

            // Not every cell holds a bird — density falls off from the center.
            vec2 cpos = (id + rnd) * cell;
            float dc = length(cpos - center) / max(spread, 1e-3);
            float present = step(rnd.x, exp(-dc * dc * 2.2));
            if (present < 0.5) continue;

            // Shared wind field, per-bird phase lag so nothing snaps in lockstep.
            float lag = rnd.y * 6.2831;
            float wind = vnoise(cpos * 2.3 + vec2(mt * 0.9, mt * 0.63)) * 6.2831
                       + bank + 0.35 * sin(mt * 1.7 + lag);
            vec2 dir = vec2(cos(wind), sin(wind));

            // Bird drifts inside its cell along the wind.
            vec2 bpos = cpos + dir * cell * 0.35 * sin(mt * 2.1 + lag);
            vec2 d = p - bpos;

            float len = cell * 0.34 * birdScale * depth;
            float wid = cell * (0.10 + 0.05 * depth) * birdScale;

            // Wingbeat flicker: highs make sparse birds strobe their stroke width.
            float flick = 1.0 + highP * 0.8 * step(0.82, rnd.y)
                        * sin(TIME * 14.0 + lag * 3.0);
            wid *= clamp(flick, 0.4, 1.9);

            float b = birdDash(d, dir, len, wid) * depth;

            // Faint motion ghost trailing behind — one wingbeat of memory.
            b += ghosting * 0.35 * birdDash(d + dir * len * 1.8, dir, len * 0.8, wid) * depth;

            v = max(v, b);
        }
    }

    // Quiet sky: a barely-there vertical glow so silence isn't a dead frame;
    // it also swells with loudness so the whole frame tracks the music.
    float sky = skyGlow * (1.0 - length(p) * 0.7) * (0.45 + 0.35 * drive + 0.9 * levelP);
    v = clamp(v * (0.60 + 0.20 * drive + 0.45 * levelP + 0.10 * beat * beat) + sky, 0.0, 1.0);

    v = mix(v, 1.0 - v, clamp(invert, 0.0, 1.0));
    gl_FragColor = vec4(vec3(v), 1.0);
}
