/*{
  "CATEGORIES": [
    "3D",
    "Generator",
    "Audio Reactive"
  ],
  "DESCRIPTION": "PBR Reference 3D — a curated luxury museum piece. One BRDF, four art-historical moods: Kapoor's chrome sphere, Brancusi's polished bronze, Klein's IKB monochromes, Judd's anodized stack. Real Cook-Torrance GGX + Schlick fresnel; per-mood SDF scenes, lighting rigs, and procedural environments. Bass breathes the form, mid drives slow monumental orbit, treble shimmers reflections. Returns LINEAR HDR — host applies ACES.",
  "INPUTS": [
    {
      "NAME": "mood",
      "LABEL": "Mood",
      "TYPE": "long",
      "DEFAULT": 0,
      "VALUES": [
        0,
        1,
        2,
        3
      ],
      "LABELS": [
        "Kapoor Mirror",
        "Brancusi Bronze",
        "Klein Monochrome",
        "Judd Stack"
      ]
    },
    {
      "NAME": "keyAngle",
      "LABEL": "Key Light Angle",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 6.2832,
      "DEFAULT": 0.785
    },
    {
      "NAME": "keyElevation",
      "LABEL": "Key Elevation",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1.5708,
      "DEFAULT": 0.7
    },
    {
      "NAME": "ambient",
      "LABEL": "Ambient",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 0.5,
      "DEFAULT": 0.08
    },
    {
      "NAME": "rimStrength",
      "LABEL": "Rim Strength",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1.5,
      "DEFAULT": 0.5
    },
    {
      "NAME": "exposure",
      "LABEL": "Exposure",
      "TYPE": "float",
      "MIN": 0.3,
      "MAX": 3,
      "DEFAULT": 1
    },
    {
      "NAME": "roughnessTrim",
      "LABEL": "Roughness Trim",
      "TYPE": "float",
      "MIN": -0.3,
      "MAX": 0.3,
      "DEFAULT": 0
    },
    {
      "NAME": "metalnessTrim",
      "LABEL": "Metalness Trim",
      "TYPE": "float",
      "MIN": -0.3,
      "MAX": 0.3,
      "DEFAULT": 0
    },
    {
      "NAME": "camOrbitSpeed",
      "LABEL": "Orbit Speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 0.18,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "keyColor",
      "LABEL": "Key Light",
      "TYPE": "color",
      "DEFAULT": [
        1,
        0.94,
        0.82,
        1
      ],
      "GROUP": "Color"
    },
    {
      "NAME": "fillColor",
      "LABEL": "Fill Light",
      "TYPE": "color",
      "DEFAULT": [
        0.55,
        0.7,
        1,
        1
      ],
      "GROUP": "Color"
    },
    {
      "NAME": "accentA",
      "LABEL": "Accent A",
      "TYPE": "color",
      "DEFAULT": [
        0.06,
        0.1,
        0.78,
        1
      ],
      "GROUP": "Color"
    },
    {
      "NAME": "accentB",
      "LABEL": "Accent B",
      "TYPE": "color",
      "DEFAULT": [
        0.92,
        0.72,
        0.22,
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
      "NAME": "camDist",
      "LABEL": "Camera Distance",
      "TYPE": "float",
      "MIN": 1.5,
      "MAX": 12,
      "DEFAULT": 4.5,
      "GROUP": "Camera / Layout"
    },
    {
      "NAME": "camHeight",
      "LABEL": "Camera Height",
      "TYPE": "float",
      "MIN": -3,
      "MAX": 4,
      "DEFAULT": 1.2,
      "GROUP": "Camera / Layout"
    },
    {
      "NAME": "camAzimuth",
      "LABEL": "Camera Azimuth",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 6.2832,
      "DEFAULT": 0,
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
      "LABEL": "Audio React",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 1,
      "GROUP": "Audio Reactivity"
    }
  ]
}*/

// ════════════════════════════════════════════════════════════════════════
//  PBR REFERENCE 3D — PBR as a CURATORIAL medium, not a tutorial.
//  Single Cook-Torrance GGX BRDF, four art-historical worlds:
//    0  Kapoor Mirror     — chrome sphere, warm studio.
//    1  Brancusi Bronze   — polished bronze on walnut plinth, golden hour.
//    2  Klein Monochrome  — three IKB cubes in saturated ultramarine air.
//    3  Judd Stack        — five anodized boxes, cool industrial light.
//  Universal camera + lighting uniforms shared with siblings.
// ════════════════════════════════════════════════════════════════════════

#define MAX_STEPS 110
#define MAX_DIST  40.0
#define EPS       0.0007
#define PI        3.14159265

// ─── SDF primitives ────────────────────────────────────────────────────
float sdSphere(vec3 p, float r) { return length(p) - r; }
float sdBox(vec3 p, vec3 b)     { vec3 q = abs(p) - b; return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0); }
float sdRoundBox(vec3 p, vec3 b, float r) { vec3 q = abs(p) - b; return length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0) - r; }
float sdCapsule(vec3 p, float h, float r) {
    p.y -= clamp(p.y, -h, h);
    return length(p) - r;
}
float opSU(float a, float b, float k) {
    float h = clamp(0.5 + 0.5*(b-a)/k, 0.0, 1.0);
    return mix(b,a,h) - k*h*(1.0-h);
}

struct Hit { float d; float id; };
Hit closer(Hit a, Hit b) { if (a.d < b.d) { return a; } return b; }

// ─── Scenes (id 0 = pedestal/ground, 1+ = primary art) ─────────────────

// 0: Kapoor — single chrome sphere on a warm timber ground
Hit sceneKapoor(vec3 p, float breath) {
    float r  = 1.05 * (1.0 + 0.012 * breath);
    float s  = sdSphere(p - vec3(0.0, r - 0.05, 0.0), r);
    float gr = p.y + 0.05;
    return closer(Hit(s, 1.0), Hit(gr, 0.0));
}

// 1: Brancusi — capsule + sphere stacked vertically, walnut plinth + floor
Hit sceneBrancusi(vec3 p, float breath) {
    float scale = 1.0 + 0.010 * breath;
    vec3  q     = p / scale;
    float capA  = sdCapsule(q - vec3(0.0, 0.55, 0.0), 0.55, 0.22);
    float capB  = sdCapsule(q - vec3(0.0, 0.35, 0.0), 0.35, 0.30);
    float shaft = opSU(capA, capB, 0.18);
    float orb   = sdSphere(q - vec3(0.0, 1.45, 0.0), 0.22);
    float form  = opSU(shaft, orb, 0.14) * scale;
    float plinth = sdRoundBox(p - vec3(0.0, -0.50, 0.0), vec3(0.6, 0.45, 0.6), 0.02);
    float floorD = p.y + 1.0;
    Hit h = Hit(form, 1.0);
    h = closer(h, Hit(plinth, 0.0));
    h = closer(h, Hit(floorD, 0.0));
    return h;
}

// 2: Klein — three IKB cubes at varying heights
Hit sceneKlein(vec3 p, float breath) {
    float s = 1.0 + 0.008 * breath;
    float a = sdRoundBox((p - vec3(-0.95, 0.30, 0.10)) / s, vec3(0.40), 0.025) * s;
    float b = sdRoundBox((p - vec3( 0.05, 0.65,-0.15)) / s, vec3(0.42), 0.025) * s;
    float c = sdRoundBox((p - vec3( 1.05, 0.42, 0.20)) / s, vec3(0.38), 0.025) * s;
    float gr = p.y + 0.12;
    Hit h = Hit(a, 1.0);
    h = closer(h, Hit(b, 2.0));
    h = closer(h, Hit(c, 3.0));
    h = closer(h, Hit(gr, 0.0));
    return h;
}

// 3: Judd — five horizontal anodized boxes stacked vertically with gaps
Hit sceneJudd(vec3 p, float breath) {
    float s = 1.0 + 0.006 * breath;
    vec3  bsize = vec3(0.95, 0.13, 0.42);
    float gap   = 0.085;
    float pitch = (bsize.y * 2.0) + gap;
    float y0    = -2.0 * pitch;
    Hit h = Hit(1e9, -1.0);
    for (int i = 0; i < 5; i++) {
        float yi = y0 + float(i) * pitch + 0.40;
        float d  = sdRoundBox((p - vec3(0.0, yi, 0.0)) / s, bsize, 0.018) * s;
        h = closer(h, Hit(d, float(i + 1)));
    }
    float floorD = p.y + 0.95;
    h = closer(h, Hit(floorD, 0.0));
    return h;
}

Hit map(vec3 p, int m, float breath) {
    if (m == 0) return sceneKapoor(p, breath);
    if (m == 1) return sceneBrancusi(p, breath);
    if (m == 2) return sceneKlein(p, breath);
    return sceneJudd(p, breath);
}

vec3 calcNormal(vec3 p, int m, float breath) {
    const vec2 e = vec2(EPS, 0.0);
    return normalize(vec3(
        map(p + e.xyy, m, breath).d - map(p - e.xyy, m, breath).d,
        map(p + e.yxy, m, breath).d - map(p - e.yxy, m, breath).d,
        map(p + e.yyx, m, breath).d - map(p - e.yyx, m, breath).d));
}

// ─── Per-mood procedural environment (cheap IBL probe) ─────────────────
vec3 envSample(vec3 d, int m, float shimmer, vec3 aA, vec3 aB) {
    float h = clamp(d.y * 0.5 + 0.5, 0.0, 1.0);
    if (m == 0) {
        // Kapoor — warm cream studio, pinkish horizon, timber floor
        vec3 sky    = vec3(1.55, 1.42, 1.20);
        vec3 horiz  = vec3(1.10, 0.78, 0.62);
        vec3 ground = vec3(0.55, 0.32, 0.22);
        vec3 c = (d.y > 0.0) ? mix(horiz, sky,    smoothstep(0.0, 0.6,  d.y))
                             : mix(horiz, ground, smoothstep(0.0, 0.5, -d.y));
        float key = pow(max(dot(d, normalize(vec3(0.55, 0.65, 0.45))), 0.0), 18.0);
        c += vec3(1.4, 1.25, 1.05) * key * 0.45;
        return c * (1.0 + shimmer * 0.05);
    }
    if (m == 1) {
        // Brancusi — golden hour: warm umber sky, walnut floor
        vec3 sky    = vec3(0.75, 0.55, 0.30);
        vec3 horiz  = vec3(0.95, 0.62, 0.32);
        vec3 ground = vec3(0.10, 0.07, 0.05);
        vec3 c = (d.y > 0.0) ? mix(horiz, sky,    smoothstep(0.0, 0.7,  d.y))
                             : mix(horiz, ground, smoothstep(0.0, 0.5, -d.y));
        float win = pow(max(dot(d, normalize(vec3(-0.8, 0.45, -0.3))), 0.0), 28.0);
        c += vec3(1.6, 1.10, 0.55) * win * 0.55;
        return c * (1.0 + shimmer * 0.04);
    }
    if (m == 2) {
        // Klein — saturated ultramarine air, accentA tints the dome
        vec3 base   = aA * 1.3;
        vec3 sky    = base * 0.85 + vec3(0.02, 0.04, 0.10);
        vec3 horiz  = base * 1.15;
        vec3 ground = base * 0.55;
        vec3 c = (d.y > 0.0) ? mix(horiz, sky,    smoothstep(0.0, 0.8,  d.y))
                             : mix(horiz, ground, smoothstep(0.0, 0.6, -d.y));
        float accent = pow(max(dot(d, normalize(vec3(0.3, 0.7, 0.55))), 0.0), 12.0);
        c += aA * accent * 0.45;
        return c * (1.0 + shimmer * 0.03);
    }
    // 3: Judd — cool industrial fluorescent grey-blue
    vec3 sky    = vec3(0.62, 0.68, 0.72);
    vec3 horiz  = vec3(0.50, 0.55, 0.60);
    vec3 ground = vec3(0.18, 0.20, 0.22);
    vec3 c = (d.y > 0.0) ? mix(horiz, sky,    smoothstep(0.0, 0.6,  d.y))
                         : mix(horiz, ground, smoothstep(0.0, 0.5, -d.y));
    float strip = pow(max(d.y, 0.0), 6.0);
    c += vec3(0.95, 1.05, 1.10) * strip * 0.30;
    return c * (1.0 + shimmer * 0.06);
}

// ─── Cook-Torrance GGX ─────────────────────────────────────────────────
float ndfGGX(float ndh, float a) {
    float a2 = a * a;
    float denom = (ndh * ndh) * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom + 1e-5);
}
float gSmith(float ndv, float ndl, float a) {
    float k = (a + 1.0); k = (k * k) * 0.125;
    float gv = ndv / (ndv * (1.0 - k) + k);
    float gl = ndl / (ndl * (1.0 - k) + k);
    return gv * gl;
}
vec3 fSchlick(float vdh, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - vdh, 5.0);
}

// ─── Material lookup per mood + id ─────────────────────────────────────
struct Material { vec3 albedo; float roughness; float metallic; };

Material material(int mood, float id, vec3 aA, vec3 aB, float rTrim, float mTrim) {
    Material mat;
    if (mood == 0) {
        if (id < 0.5) {
            // Kapoor ground — warm timber
            mat.albedo    = vec3(0.42, 0.26, 0.18);
            mat.roughness = 0.55;
            mat.metallic  = 0.0;
        } else {
            // Kapoor sphere — chrome mirror
            mat.albedo    = vec3(0.92, 0.93, 0.95);
            mat.roughness = 0.025;
            mat.metallic  = 1.0;
        }
    } else if (mood == 1) {
        if (id < 0.5) {
            // walnut plinth + dark floor
            mat.albedo    = vec3(0.18, 0.10, 0.06);
            mat.roughness = 0.45;
            mat.metallic  = 0.0;
        } else {
            // polished bronze, accentB nudges the gold tint
            mat.albedo    = mix(vec3(0.92, 0.70, 0.42), aB, 0.35);
            mat.roughness = 0.10;
            mat.metallic  = 1.0;
        }
    } else if (mood == 2) {
        // Klein — IKB suede, cubes use accentA, ground darker
        if (id < 0.5) {
            mat.albedo    = aA * 0.45;
            mat.roughness = 0.85;
            mat.metallic  = 0.0;
        } else {
            mat.albedo    = aA;
            mat.roughness = 0.70;
            mat.metallic  = 0.0;
        }
    } else {
        // Judd — five anodized colors + cool concrete floor
        if (id < 0.5) { mat.albedo = vec3(0.32, 0.34, 0.36); mat.roughness = 0.65; mat.metallic = 0.0; }
        else if (id < 1.5) { mat.albedo = mix(vec3(0.85, 0.18, 0.16), aB, 0.25); mat.roughness = 0.30; mat.metallic = 0.85; }
        else if (id < 2.5) { mat.albedo = mix(vec3(0.92, 0.72, 0.22), aB, 0.40); mat.roughness = 0.32; mat.metallic = 0.85; }
        else if (id < 3.5) { mat.albedo = vec3(0.88, 0.55, 0.32); mat.roughness = 0.30; mat.metallic = 0.85; }
        else if (id < 4.5) { mat.albedo = vec3(0.78, 0.80, 0.82); mat.roughness = 0.28; mat.metallic = 0.90; }
        else                { mat.albedo = mix(vec3(0.22, 0.28, 0.62), aA, 0.30); mat.roughness = 0.32; mat.metallic = 0.85; }
    }
    mat.roughness = clamp(mat.roughness + rTrim, 0.02, 1.0);
    mat.metallic  = clamp(mat.metallic  + mTrim, 0.0,  1.0);
    return mat;
}

// ─── Per-mood key/fill rig — modulated by uniforms ─────────────────────
void lightingRig(int mood, vec3 keyDirIn, vec3 keyColIn, vec3 fillColIn,
                 out vec3 kDir, out vec3 kCol, out vec3 fDir, out vec3 fCol) {
    if (mood == 0) {
        // Kapoor — dramatic studio key + cool fill
        kDir = keyDirIn;                    kCol = keyColIn  * 2.10;
        fDir = normalize(vec3(-0.70, 0.20,-0.40)); fCol = fillColIn * 0.75;
    } else if (mood == 1) {
        // Brancusi — warm window-light, golden hour rim, soft warm fill
        kDir = keyDirIn;                    kCol = keyColIn  * 2.40;
        fDir = normalize(vec3( 0.40, 0.30, 0.80)); fCol = fillColIn * 0.50;
    } else if (mood == 2) {
        // Klein — nearly flat, one subtle high accent
        kDir = keyDirIn;                    kCol = keyColIn  * 0.85;
        fDir = normalize(vec3(-0.40, 0.30,-0.50)); fCol = fillColIn * 0.65;
    } else {
        // Judd — cool overhead fluorescents
        kDir = keyDirIn;                    kCol = keyColIn  * 1.30;
        fDir = normalize(vec3(-0.85, 0.25, 0.30)); fCol = fillColIn * 0.65;
    }
}

// ─── Soft shadow ───────────────────────────────────────────────────────
float softShadow(vec3 ro, vec3 rd, int m, float breath) {
    float res = 1.0;
    float t   = 0.04;
    for (int i = 0; i < 24; i++) {
        float h = map(ro + rd * t, m, breath).d;
        if (h < 0.001) return 0.0;
        res = min(res, 12.0 * h / t);
        t  += clamp(h, 0.02, 0.30);
        if (t > 6.0) break;
    }
    return clamp(res, 0.0, 1.0);
}

// ─── one Cook-Torrance lobe ────────────────────────────────────────────
vec3 ctLobe(vec3 n, vec3 v, vec3 L, vec3 lCol, Material mat, vec3 F0, float ndv) {
    vec3  H  = normalize(L + v);
    float ndl = max(dot(n, L), 0.0);
    float ndh = max(dot(n, H), 0.0);
    float vdh = max(dot(v, H), 0.0);
    float D  = ndfGGX(ndh, mat.roughness);
    float G  = gSmith(ndv, ndl, mat.roughness);
    vec3  F  = fSchlick(vdh, F0);
    vec3  sp = (D * G * F) / max(4.0 * ndv * ndl, 1e-4);
    vec3  kd = (1.0 - F) * (1.0 - mat.metallic);
    return (kd * mat.albedo / PI + sp) * lCol * ndl;
}

// ─── PBR shade ─────────────────────────────────────────────────────────
vec3 shade(vec3 p, vec3 n, vec3 v, Material mat, int mood, float breath, float shimmer,
           vec3 keyDirIn, vec3 keyColIn, vec3 fillColIn,
           float ambientIn, float rimStrengthIn, vec3 aA, vec3 aB) {
    vec3 kDir, kCol, fDir, fCol;
    lightingRig(mood, keyDirIn, keyColIn, fillColIn, kDir, kCol, fDir, fCol);

    vec3  F0  = mix(vec3(0.04), mat.albedo, mat.metallic);
    float ndv = max(dot(n, v), 1e-4);
    float sh  = softShadow(p + n * 0.002, kDir, mood, breath);
    vec3  direct = ctLobe(n, v, kDir, kCol, mat, F0, ndv) * sh;
    vec3  fill   = ctLobe(n, v, fDir, fCol, mat, F0, ndv);

    // indirect — environment probe (cheap IBL)
    vec3 R    = reflect(-v, n);
    vec3 envD = envSample(n, mood, shimmer, aA, aB);
    vec3 envS = envSample(R, mood, shimmer, aA, aB);
    vec3 iblDiff = envD * mat.albedo * (1.0 - mat.metallic) * 0.45;
    vec3 iblSpec = envS * F0 * mix(1.0, 0.10, mat.roughness);
    iblSpec *= (1.0 + shimmer * (1.0 - mat.roughness) * 0.6);

    // ambient floor — uniform-controlled
    vec3 amb = mat.albedo * (1.0 - mat.metallic) * ambientIn * 1.8;

    // rim
    float rim = pow(1.0 - ndv, 4.0);
    vec3  rimCol = (mood == 2) ? aA * 2.4
                  : (mood == 1) ? vec3(1.30, 0.85, 0.45)
                  : (mood == 0) ? vec3(1.10, 1.00, 0.95)
                                : vec3(0.95, 1.00, 1.10);
    vec3 rimL = rimCol * rim * rimStrengthIn * 0.36 * (1.0 - mat.roughness * 0.6);

    return direct + fill + iblDiff + iblSpec + amb + rimL;
}

// ─── Audio scaffolding — live uniforms, soft knees (playbook), with a
//     gentle sine idle floor so the piece still breathes in silence ─────
float bassPulse(float aR)  {
    // Sub coupling + low floor: hiphop's sparse kicks live in audioSub and
    // sat below the old 0.05..0.85^1.6 knee. Gentler curve so soft jazz
    // accents register too.
    float bP = pow(smoothstep(0.03, 0.85, audioBass + 0.6 * audioSub), 1.3);
    return 0.5 + (0.10 * sin(TIME * 1.6) + 0.40 * bP) * aR * 0.9;
}
float midDrift(float aR)   {
    float mP = smoothstep(0.04, 0.85, audioMid);
    return 0.5 + (0.20 * sin(TIME * 0.5) + 0.30 * mP) * aR * 0.6;
}
float treShimmer(float aR) {
    float hP = pow(smoothstep(0.10, 0.90, audioHigh), 1.2);
    return 0.5 + (0.10 * sin(TIME * 4.7) + 0.40 * hP) * aR * 0.7;
}

// ─── main ──────────────────────────────────────────────────────────────
void main() {
    vec2 uv = (isf_FragNormCoord.xy * 2.0 - 1.0)
            * vec2(RENDERSIZE.x / RENDERSIZE.y, 1.0);

    int   m  = int(mood);
    float aR = clamp(audioReact, 0.0, 2.0);

    float bass    = bassPulse(aR);
    float mid     = midDrift(aR);
    float shimmer = treShimmer(aR);
    float breath  = (bass - 0.5) * 2.0;

    // Universal camera — azimuth + orbit (mid drift) + height/dist
    float ang    = camAzimuth + TIME * camOrbitSpeed + (mid - 0.5) * 0.35;
    float frameH = camHeight + (m == 1 ? 0.55 : (m == 3 ? 0.20 : 0.0));
    float frameD = camDist   + (m == 3 ? 0.40 : 0.0);
    vec3  ro = vec3(cos(ang) * frameD, frameH, sin(ang) * frameD);
    vec3  ta = (m == 1) ? vec3(0.0, 0.55, 0.0)
             : (m == 3) ? vec3(0.0, 0.30, 0.0)
                        : vec3(0.0, 0.30, 0.0);
    vec3  fw = normalize(ta - ro);
    vec3  ri = normalize(cross(vec3(0.0, 1.0, 0.0), fw));
    vec3  up = cross(fw, ri);
    vec3  rd = normalize(fw + uv.x * ri + uv.y * up);

    // Universal key light — angle + elevation in world space
    float ce = cos(keyElevation);
    vec3  keyDirIn  = normalize(vec3(cos(keyAngle) * ce,
                                     sin(keyElevation),
                                     sin(keyAngle) * ce));
    vec3 keyColIn  = keyColor.rgb;
    vec3 fillColIn = fillColor.rgb;
    vec3 aA = accentA.rgb;
    vec3 aB = accentB.rgb;

    // March
    float t   = 0.0;
    float idH = -1.0;
    bool  hit = false;
    for (int i = 0; i < MAX_STEPS; i++) {
        vec3 p = ro + rd * t;
        Hit  h = map(p, m, breath);
        if (h.d < EPS) { hit = true; idH = h.id; break; }
        t += h.d * 0.92;
        if (t > MAX_DIST) break;
    }

    vec3 col;
    if (hit) {
        vec3 p = ro + rd * t;
        vec3 n = calcNormal(p, m, breath);
        vec3 v = -rd;
        Material mat = material(m, idH, aA, aB, roughnessTrim, metalnessTrim);
        col = shade(p, n, v, mat, m, breath, shimmer,
                    keyDirIn, keyColIn, fillColIn, ambient, rimStrength, aA, aB);
        float fog = 1.0 - exp(-t * 0.025);
        col = mix(col, envSample(rd, m, shimmer, aA, aB), fog * 0.45);
    } else {
        col = envSample(rd, m, shimmer, aA, aB);
        // universal background override (a=0 -> untouched environment)
        col = mix(col, bgColor.rgb, bgColor.a);
    }

    // Museum vignette
    col *= 1.0 - 0.16 * length(uv * 0.55);

    // Whole-frame exposure follower (r2 ambient fix): LINEAR bass/sub + mid
    // followers — the round-1 pow/knee crushed ambient's slow swells, and
    // 0.16 depth was too shallow once the host's ACES compressed it. Plus a
    // decaying kick trace (audioBeatPulse decays 300ms+). The geometric
    // breath (~1%) is far too subtle to score; luminance is the visible
    // channel for this museum piece. Silence -> 0.97 as before.
    float bassF = smoothstep(0.02, 0.95, audioBass + 0.5 * audioSub);
    float midF  = smoothstep(0.02, 0.95, audioMid);
    float hitT  = clamp(audioBeatPulse, 0.0, 1.0);
    col *= exposure * (0.97 + (0.28 * bassF + 0.16 * midF + 0.12 * hitT) * aR);

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
    gl_FragColor = vec4(uc, 1.0);
}
