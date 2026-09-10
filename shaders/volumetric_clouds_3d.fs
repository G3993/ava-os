/*{
  "CATEGORIES": [
    "3D",
    "Generator",
    "Atmosphere",
    "Audio Reactive"
  ],
  "DESCRIPTION": "Volumetric cloudscape as painting — Turner's stormy sunsets, Rothko's horizon stripes, Constable's cumulus, Friedrich's mountain mist. Single-scatter raymarch with Beer-Lambert extinction and Henyey-Greenstein phase. Bass drives wind drift; mid pushes density; treble adds atmospheric particulates. Stays alive in silence. Output linear HDR.",
  "INPUTS": [
    {
      "NAME": "density",
      "LABEL": "Density",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 4,
      "DEFAULT": 1.6,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "coverage",
      "LABEL": "Coverage",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.55,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "cloudHeight",
      "LABEL": "Cloud Height",
      "TYPE": "float",
      "MIN": -2,
      "MAX": 2,
      "DEFAULT": 0,
      "GROUP": "Shape / Geometry"
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
      "NAME": "windSpeed",
      "LABEL": "Wind",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.14,
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
      "NAME": "exposure",
      "LABEL": "Exposure",
      "TYPE": "float",
      "MIN": 0.3,
      "MAX": 3,
      "DEFAULT": 1,
      "GROUP": "Color"
    },
    {
      "NAME": "silverLining",
      "LABEL": "Silver Lining",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 2,
      "DEFAULT": 1,
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
      "NAME": "mood",
      "LABEL": "Mood",
      "TYPE": "long",
      "VALUES": [
        0,
        1,
        2,
        3
      ],
      "LABELS": [
        "Turner Sunset",
        "Rothko Horizon",
        "Constable Day",
        "Friedrich Mist"
      ],
      "DEFAULT": 0
    },
    {
      "NAME": "anisotropy",
      "LABEL": "Forward Scatter",
      "TYPE": "float",
      "MIN": -0.95,
      "MAX": 0.95,
      "DEFAULT": 0.62
    }
  ]
}*/

// ════════════════════════════════════════════════════════════════════════
//   CLOUDSCAPE AS PAINTING
//   Turner · Rothko · Constable · Friedrich
//
//   Single-pass volumetric raymarch with single-scatter lighting. Beer-
//   Lambert extinction along view + light rays, Henyey-Greenstein phase
//   for forward-scatter "silver-lining" highlights at cloud edges.
//
//   Each mood is a distinct PALETTE + DENSITY PROFILE + HORIZON BEHAVIOR.
//   Camera + key/fill/ambient lighting + exposure are universal controls.
// ════════════════════════════════════════════════════════════════════════

#define V_STEPS 36
#define L_STEPS 6
#define PI 3.14159265

// ---------- noise ------------------------------------------------------
float hash13(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }

float vnoise3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    float n000 = hash13(i + vec3(0,0,0));
    float n100 = hash13(i + vec3(1,0,0));
    float n010 = hash13(i + vec3(0,1,0));
    float n110 = hash13(i + vec3(1,1,0));
    float n001 = hash13(i + vec3(0,0,1));
    float n101 = hash13(i + vec3(1,0,1));
    float n011 = hash13(i + vec3(0,1,1));
    float n111 = hash13(i + vec3(1,1,1));
    return mix(mix(mix(n000,n100,f.x),mix(n010,n110,f.x),f.y),
               mix(mix(n001,n101,f.x),mix(n011,n111,f.x),f.y), f.z);
}

float fbm3(vec3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * vnoise3(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

// ---------- mood palette -----------------------------------------------
struct Mood {
    vec3 zenith;
    vec3 horizon;
    float densMul;
    float covOff;
    float slabLo;
    float slabHi;
    float aniso;
    float ambientGain;
    float verticalShape;  // 0 = flat strata, 1 = puffy cumulus, 2 = uniform fog
};

Mood selectMood(int m) {
    Mood o;
    if (m == 0) {
        // TURNER SUNSET — dramatic stormy orange-red rim, dark underbelly
        o.zenith       = vec3(0.06, 0.10, 0.22);
        o.horizon      = vec3(1.35, 0.55, 0.18);
        o.densMul      = 1.35;
        o.covOff       = 0.05;
        o.slabLo       = 1.6;
        o.slabHi       = 6.5;
        o.aniso        = 0.78;
        o.ambientGain  = 0.18;
        o.verticalShape= 1.0;
    } else if (m == 1) {
        // ROTHKO HORIZON — stratified rust/amber/ivory bands
        o.zenith       = vec3(0.42, 0.18, 0.10);
        o.horizon      = vec3(1.55, 0.95, 0.55);
        o.densMul      = 1.10;
        o.covOff       = 0.12;
        o.slabLo       = 2.4;
        o.slabHi       = 5.0;
        o.aniso        = 0.55;
        o.ambientGain  = 0.32;
        o.verticalShape= 0.0;
    } else if (m == 2) {
        // CONSTABLE DAY — white cumulus on cobalt
        o.zenith       = vec3(0.18, 0.42, 0.78);
        o.horizon      = vec3(0.78, 0.85, 0.92);
        o.densMul      = 0.90;
        o.covOff       = -0.12;
        o.slabLo       = 2.2;
        o.slabHi       = 5.6;
        o.aniso        = 0.45;
        o.ambientGain  = 0.45;
        o.verticalShape= 1.5;
    } else {
        // FRIEDRICH MIST — cold uniform fog, peaks barely visible
        o.zenith       = vec3(0.62, 0.66, 0.70);
        o.horizon      = vec3(0.82, 0.82, 0.80);
        o.densMul      = 0.75;
        o.covOff       = 0.30;
        o.slabLo       = 0.6;
        o.slabHi       = 5.4;
        o.aniso        = 0.18;
        o.ambientGain  = 0.55;
        o.verticalShape= 2.0;
    }
    return o;
}

// ---------- cloud density ---------------------------------------------
float cloudDensity(vec3 p, Mood mo, float wind, float bass, float mid, float treble) {
    if (p.y < mo.slabLo || p.y > mo.slabHi) return 0.0;

    // R3: audio must NOT multiply TIME — `TIME * (0.45 + 0.85*bass)` made the
    // drift PHASE jump by TIME*Δbass (tens of noise-units mid-run), a slosh
    // whose steps decorrelated from the envelope (ambient corr 0.000, null
    // 0.958). Audio now nudges phase with a BOUNDED lean that eases back as
    // bass decays. Silence = TIME*0.45 exactly (unchanged look).
    // R3b MEASURED: the 2.4 lean made drift VELOCITY = 0.45 + 2.4*dBass/dt,
    // and ambient's dBass/dt swings ±0.55 — velocity reversed through ZERO at
    // envelope-locked phases, so ambient frames moved LESS than silence
    // (med 0.0045 < sil 0.0049) and corr nulled to 0.000. Lean capped at 0.6
    // so velocity stays in [0.12, 0.78] — never reverses, never freezes.
    float wt = TIME * 0.45 + 0.6 * bass;
    p.x += wind * wt;
    p.z += wind * wt * 0.55;

    float base   = fbm3(p * 0.42);
    float detail = fbm3(p * 1.55 + vec3(treble * 0.35, 0.0, treble * 0.25)) * 0.35;
    float n = base - detail;

    float h = (p.y - mo.slabLo) / (mo.slabHi - mo.slabLo);
    float vertical;
    if (mo.verticalShape < 0.5) {
        float band = sin(h * 3.14159 * 2.5) * 0.5 + 0.5;
        vertical = mix(0.55, 1.0, band) * smoothstep(0.0, 0.15, h) * smoothstep(1.0, 0.85, h);
    } else if (mo.verticalShape < 1.25) {
        vertical = smoothstep(0.0, 0.25, h) * smoothstep(1.0, 0.55, h);
    } else if (mo.verticalShape < 1.75) {
        vertical = smoothstep(0.0, 0.18, h) * smoothstep(1.0, 0.45, h);
        vertical = pow(vertical, 0.85);
    } else {
        vertical = smoothstep(0.0, 0.05, h) * smoothstep(1.0, 0.92, h);
        vertical = mix(0.7, 1.0, vertical);
    }

    // R3b MEASURED: the mid→coverage/density push ANTI-correlated — thicker
    // slab saturates the cloud interior (trans→0 sooner), so frames moved
    // LESS with audio than in silence (med < sil in all 5 styles) and it
    // cancelled the display breath (ambient corr 0.000). Density stays
    // audio-neutral; the response lives in the whole-canvas breath instead.
    float covEff = clamp(coverage + mo.covOff, 0.0, 1.0);
    float d = (n - (1.0 - covEff)) * vertical;
    return clamp(d, 0.0, 1.0) * density * mo.densMul;
}

// ---------- phase function --------------------------------------------
float phaseHG(float ct, float g) {
    return (1.0 - g*g) / (4.0 * PI * pow(1.0 + g*g - 2.0*g*ct, 1.5));
}

// ---------- light march -----------------------------------------------
float lightMarch(vec3 p, vec3 ldir, Mood mo, float wind, float bass, float mid, float treble) {
    float t = 0.0;
    float trans = 1.0;
    for (int i = 0; i < L_STEPS; i++) {
        vec3 sp = p + ldir * t;
        float d = cloudDensity(sp, mo, wind, bass, mid, treble);
        trans *= exp(-d * 0.55);
        t += 0.42 + float(i) * 0.18;
    }
    return trans;
}

// ---------- sky background --------------------------------------------
vec3 skyGradient(vec3 rd, Mood mo) {
    float t = clamp(rd.y * 1.4 + 0.18, 0.0, 1.0);
    t = pow(t, 0.85);
    return mix(mo.horizon, mo.zenith, t);
}

// ---------- main ------------------------------------------------------
void main() {
    vec2 res = RENDERSIZE.xy;
    vec2 fc = (gl_FragCoord.xy - 0.5*res) / res.y;

    float bass   = clamp(audioBass, 0.0, 1.0) * audioReact;
    float mid    = clamp(audioMid,  0.0, 1.0) * audioReact;
    float treble = clamp(audioHigh, 0.0, 1.0) * audioReact;

    // Continuous smooth band-followers (ambient fix) driving the
    // whole-canvas luminance breath below. Round-2: LINEAR bands — the
    // round-1 pow/smoothstep knee halved ambient's 0.1-0.8 swells and the
    // detector read 0. Bands are already smoothed upstream.
    float react  = clamp(audioReact, 0.0, 2.0);
    float bassSm = clamp(audioBass, 0.0, 1.0) * react;
    float midSm  = clamp(audioMid,  0.0, 1.0) * react;
    float highSm = clamp(audioHigh, 0.0, 1.0) * react;

    Mood mo = selectMood(int(mood));

    // Apply cloudHeight offset to vertical slab (look-pusher #1).
    mo.slabLo += cloudHeight;
    mo.slabHi += cloudHeight;

    // ---------- UNIVERSAL CAMERA -----------------------------------
    // camDist = horizontal distance to scene focus
    // camHeight = height above ground
    // camAzimuth + orbit drives heading
    float az = camAzimuth + TIME * camOrbitSpeed;
    vec3 focus = vec3(0.0, max(mo.slabLo, 1.5), 0.0);
    vec3 ro = focus + vec3(sin(az) * camDist, camHeight, cos(az) * camDist);
    vec3 fwd = normalize(focus + vec3(0.0, 0.6, 0.0) - ro);
    vec3 rgt = normalize(cross(fwd, vec3(0,1,0)));
    vec3 up  = cross(rgt, fwd);
    vec3 rd  = normalize(fwd + rgt * fc.x + up * fc.y);

    // ---------- UNIVERSAL KEY LIGHT (sun) --------------------------
    vec3 ldir = normalize(vec3(
        cos(keyElevation) * cos(keyAngle),
        sin(keyElevation),
        cos(keyElevation) * sin(keyAngle)
    ));

    float effAniso = clamp(mo.aniso * 0.65 + anisotropy * 0.35, -0.95, 0.95);

    // ---------- SKY + SUN -------------------------------------------
    vec3 sky = skyGradient(rd, mo);
    // universal background: blend the sky backdrop toward bgColor
    sky = mix(sky, bgColor.rgb, bgColor.a);

    float horizonBand = exp(-pow(rd.y * 6.5, 2.0));
    sky += mo.horizon * horizonBand * 0.35;

    float sunDot = max(dot(rd, ldir), 0.0);
    float disc   = pow(sunDot, 480.0);
    float corona = pow(sunDot, 32.0);
    float halo   = pow(sunDot, 6.0);

    float sunBoost = (mood < 0.5) ? 9.0 :
                     (mood < 1.5) ? 6.0 :
                     (mood < 2.5) ? 5.0 :
                                    1.6;
    float coronaBoost = (mood < 0.5) ? 0.85 :
                        (mood < 1.5) ? 1.10 :
                        (mood < 2.5) ? 0.55 :
                                       0.95;
    float haloBoost   = (mood < 0.5) ? 0.35 :
                        (mood < 1.5) ? 0.55 :
                        (mood < 2.5) ? 0.20 :
                                       0.45;

    vec3 keyRGB = keyColor.rgb;
    sky += keyRGB * disc   * sunBoost;
    sky += keyRGB * corona * coronaBoost;
    sky += keyRGB * halo   * haloBoost;

    // ---------- VOLUMETRIC SLAB -------------------------------------
    float tEnter = (rd.y > 0.001) ? (mo.slabLo - ro.y) / rd.y : 1e9;
    float tExit  = (rd.y > 0.001) ? (mo.slabHi - ro.y) / rd.y : 1e9;
    if (tEnter > tExit) { float tmp = tEnter; tEnter = tExit; tExit = tmp; }
    tEnter = max(tEnter, 0.0);
    tExit  = min(tExit, 32.0);

    vec3 col = sky;
    if (tExit > tEnter && rd.y > 0.0) {
        float dt = (tExit - tEnter) / float(V_STEPS);
        float jitter = hash13(vec3(fc * 137.0, TIME * 0.3));
        float t = tEnter + dt * jitter;
        float trans = 1.0;
        vec3  scat  = vec3(0.0);
        float gPhase = phaseHG(dot(rd, ldir), effAniso);

        // Fill light = sky scattering (bluish bounce from atmosphere).
        // Ambient = cloud underbelly indirect (mood-tinted bounce).
        vec3 fill = fillColor.rgb * mo.ambientGain * 0.5;
        vec3 ambi = mix(mo.horizon, mo.zenith, 0.45) * mo.ambientGain + vec3(ambient);

        for (int i = 0; i < V_STEPS; i++) {
            if (trans < 0.01) break;
            vec3 p = ro + rd * t;
            float d = cloudDensity(p, mo, windSpeed, bass, mid, treble);
            if (d > 0.001) {
                float lt = lightMarch(p, ldir, mo, windSpeed, bass, mid, treble);
                vec3 li = keyRGB * lt * gPhase;
                // Silver lining (look-pusher #2) modulates rim edge intensity.
                float edge = pow(lt, 4.0);
                li += keyRGB * edge * 0.35 * silverLining * rimStrength;
                li += fill;
                vec3 stepCol = (li + ambi) * d * dt;
                scat += stepCol * trans;
                trans *= exp(-d * dt * 1.18);
            }
            t += dt;
        }
        col = mix(scat, sky, trans);
    }

    // ---------- ATMOSPHERIC PARTICULATES (treble) -------------------
    if (treble > 0.05) {
        float partGain = (mood < 0.5) ? 0.04 :
                         (mood < 1.5) ? 0.02 :
                         (mood < 2.5) ? 0.05 :
                                        0.08;
        float p1 = hash13(vec3(gl_FragCoord.xy * 0.5, floor(TIME * 8.0)));
        float p2 = hash13(vec3(gl_FragCoord.xy * 0.31, floor(TIME * 8.0) + 17.0));
        float mote = step(0.992, p1) * (0.5 + 0.5 * p2);
        col += vec3(0.85, 0.82, 0.75) * mote * treble * partGain;
    }

    // ---------- HORIZON-WEIGHT (Rothko gravity) ---------------------
    if (mood < 1.5) {
        float lower = smoothstep(0.05, -0.20, rd.y);
        col = mix(col, col * vec3(1.10, 0.95, 0.78) + mo.horizon * 0.08, lower * 0.35);
    }

    // ---------- subtle vignette for painterly framing ---------------
    vec2 nv = (gl_FragCoord.xy / res) - 0.5;
    float vig = 1.0 - dot(nv, nv) * 0.55;
    col *= vig;

    // Whole-canvas luminance breath — R3: COMPOSITE linear follower matching
    // the bus band mix (bass:mid:high ≈ 0.4:0.25:0.15). A bass+mid-only pair
    // is shift-degenerate against ambient's pure-sine swells; the third band
    // breaks that symmetry. Unity gain in silence.
    col *= 1.0 + 0.28 * bassSm + 0.17 * midSm + 0.10 * highSm;

    // Universal exposure.
    col *= exposure;

    // ---- universal color block (defaults = no-op) ----
    vec3 uc = col;
    float ucL = dot(uc, vec3(0.299, 0.587, 0.114));
    uc = mix(vec3(ucL), uc, colorBoost);                     // saturation
    if (hueShift > 0.0005) {                                  // cheap hue rotate (YIQ)
        float hA = hueShift * 6.2831853;
        float hC = cos(hA), hS = sin(hA);
        mat3 hM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
                + hC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
                + hS * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
        uc = clamp(hM * uc, 0.0, 1.0);
    }
    col = uc;

    gl_FragColor = vec4(col, 1.0);
}
