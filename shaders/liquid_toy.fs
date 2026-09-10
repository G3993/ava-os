/*{
  "DESCRIPTION": "Liquid Toy — a glowing droplet orbits a white dish, painting a fake-fluid heightmap that expands, swirls with fbm turbulence and fades, shaded with backlight, specular and a rainbow fringe. Bass fattens the paint brush, loudness stretches the liquid's memory, highs shimmer the rainbow.",
  "CREDIT": "Liquid toy by Leon Denise (2022), CC — procedural-noise ShaderClaw audio port",
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
      "NAME": "dropSize",
      "LABEL": "Drop Size",
      "TYPE": "float",
      "DEFAULT": 1.0,
      "MIN": 0.4,
      "MAX": 2.5
    },
    {
      "NAME": "fadeAmt",
      "LABEL": "Liquid Memory",
      "TYPE": "float",
      "DEFAULT": 0.4,
      "MIN": 0.15,
      "MAX": 0.8
    },
    {
      "NAME": "rainbowAmt",
      "LABEL": "Rainbow",
      "TYPE": "float",
      "GROUP": "Color",
      "DEFAULT": 1.0,
      "MIN": 0.0,
      "MAX": 2.0
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
    }
  ],
  "PASSES": [
    {
      "TARGET": "heightBuf",
      "PERSISTENT": true
    },
    {}
  ]
}*/

#define ss(a, b, t) smoothstep(a, b, t)

float knee(float x, float lo, float hi) { return smoothstep(lo, hi, x); }

float h31(vec3 p) {
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}
float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = h31(i), n100 = h31(i + vec3(1, 0, 0));
    float n010 = h31(i + vec3(0, 1, 0)), n110 = h31(i + vec3(1, 1, 0));
    float n001 = h31(i + vec3(0, 0, 1)), n101 = h31(i + vec3(1, 0, 1));
    float n011 = h31(i + vec3(0, 1, 1)), n111 = h31(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

// layered noise standing in for the original's noise texture
vec3 fbm3(vec3 p) {
    vec3 result = vec3(0.0);
    float amplitude = 0.5;
    for (int i = 0; i < 3; i++) {
        result += vec3(vnoise3(p / amplitude),
                       vnoise3(p / amplitude + 17.7),
                       vnoise3(p / amplitude + 41.3)) * amplitude;
        amplitude /= 3.0;
    }
    return result;
}

float heightAt(vec2 uv) { return texture2D(heightBuf, uv).r; }

vec4 passSim() {
    float T = TIME * speed;
    float ar = audioReact;
    float bassP  = pow(knee(audioBass, 0.05, 0.85), 1.6);
    float midP   = pow(knee(audioMid,  0.08, 0.90), 1.3);
    float levelP = knee(audioLevel, 0.05, 0.90);

    vec2 uv = (gl_FragCoord.xy - RENDERSIZE.xy / 2.0) / RENDERSIZE.y;
    vec2 aspect = vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);

    vec3 spice = fbm3(vec3(uv * 0.1, T * 0.01));

    // orbiting droplet; bass fattens the brush, loudness widens the orbit
    float t = T * 2.0;
    float orbitR = 0.3 + ar * 0.08 * levelP;
    vec2 duv = uv - vec2(cos(t), sin(t)) * orbitR;
    float brush = 0.1 * dropSize * (1.0 + ar * 0.6 * bassP);
    float paint = ss(brush, 0.0, length(duv));
    // a second droplet appears opposite when the music hits
    vec2 duv2 = uv + vec2(cos(t * 0.7), sin(t * 0.7)) * orbitR;
    paint = max(paint, ss(0.05 * dropSize, 0.0, length(duv2)) * ar * bassP);

    // expansion along the heightmap normal
    vec2 offset = vec2(0.0);
    uv = gl_FragCoord.xy / RENDERSIZE.xy;
    vec4 data = texture2D(heightBuf, uv);
    vec3 unit = vec3(5.0 / 472.0 / aspect, 0.0);
    vec3 normal = normalize(vec3(
        heightAt(uv - unit.xz) - heightAt(uv + unit.xz),
        heightAt(uv - unit.zy) - heightAt(uv + unit.zy),
        data.x * data.x) + 0.001);
    offset -= normal.xy;

    // turbulence (mids stir slightly, tone-level only)
    float sx = spice.x * 6.28 * 2.0 + T;
    offset += vec2(cos(sx), sin(sx)) * (1.0 + ar * 0.5 * midP);

    uv += offset / aspect / 472.0;
    float frame = texture2D(heightBuf, uv).x;

    // loudness stretches the liquid's memory
    float fade = fadeAmt * mix(1.0, 1.75, ar * levelP);
    float dt = clamp(TIMEDELTA, 0.001, 0.1);
    paint = max(paint, frame - dt * fade);
    if (FRAMEINDEX < 2) paint = 0.0;

    return vec4(clamp(paint, 0.0, 1.0));
}

vec4 passImage() {
    float T = TIME * speed;
    float ar = audioReact;
    float highP = pow(knee(audioHigh, 0.10, 0.90), 1.2);

    vec2 uv = gl_FragCoord.xy / RENDERSIZE.xy;
    float dither = h31(vec3(gl_FragCoord.xy * 0.37, fract(TIME) * 3.0));

    float gray = texture2D(heightBuf, uv).x;

    vec2 aspect = vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);
    vec3 unit = vec3(3.0 / 472.0 / aspect, 0.0);
    vec3 normal = normalize(vec3(
        heightAt(uv + unit.xz) - heightAt(uv - unit.xz),
        heightAt(uv - unit.zy) - heightAt(uv + unit.zy),
        gray * gray * gray));

    vec3 color = vec3(0.45) * (1.0 - abs(dot(normal, vec3(0, 0, 1))));

    vec3 dir = normalize(vec3(0, 1, 2));
    float specular = pow(dot(normal, dir) * 0.5 + 0.5, 20.0);
    color += vec3(0.5) * ss(0.1, 1.0, specular);

    // rainbow fringe; highs shimmer its phase
    vec3 tint = 0.5 + 0.5 * cos(vec3(1, 2, 3) * 1.0 + dot(normal, dir) * 4.0
                                - uv.y * 3.0 - 3.0 + ar * 2.0 * highP);
    color += tint * rainbowAmt * smoothstep(0.35, 0.0, gray);

    color -= dither * 0.1;

    vec3 background = vec3(1.0) * smoothstep(1.5, -0.5, length(uv - 0.5));
    color = mix(background, clamp(color, 0.0, 1.0), ss(0.01, 0.1, gray));

    // sustained loudness dims the dish slightly so quiet/loud read differently
    color *= 1.0 - ar * 0.15 * knee(audioLevel, 0.05, 0.9);
    color *= tintColor.rgb * brightness;
    return vec4(color, 1.0);
}

void main() {
    if (PASSINDEX == 0) gl_FragColor = passSim();
    else                gl_FragColor = passImage();
}
