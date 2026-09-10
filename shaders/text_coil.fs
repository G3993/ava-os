/*{
  "CATEGORIES": [
    "Generator",
    "Text"
  ],
  "DESCRIPTION": "Coil - text on spiral rings",
  "INPUTS": [
    {
      "NAME": "preset",
      "LABEL": "Style",
      "TYPE": "long",
      "VALUES": [
        0,
        1,
        2,
        3
      ],
      "LABELS": [
        "Coil Wide",
        "Coil Star",
        "Coil Lemniscate",
        "Coil Pulse"
      ],
      "DEFAULT": 0
    },
    {
      "NAME": "intensity",
      "LABEL": "Ring Size",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.5,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "density",
      "LABEL": "Rings",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.5,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "speed",
      "LABEL": "Speed",
      "TYPE": "float",
      "MIN": 0.1,
      "MAX": 3,
      "DEFAULT": 0.5,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "textColor",
      "LABEL": "Color",
      "TYPE": "color",
      "DEFAULT": [
        1,
        1,
        1,
        1
      ],
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
      "NAME": "msg",
      "TYPE": "text",
      "DEFAULT": " ETHEREA",
      "MAX_LENGTH": 48,
      "GROUP": "Text"
    },
    {
      "NAME": "fontFamily",
      "LABEL": "Font",
      "TYPE": "long",
      "VALUES": [
        0,
        1,
        2,
        3
      ],
      "LABELS": [
        "Inter",
        "Times New Roman",
        "Libre Caslon",
        "Outfit"
      ],
      "DEFAULT": 0,
      "GROUP": "Text"
    },
    {
      "NAME": "textScale",
      "LABEL": "Size",
      "TYPE": "float",
      "MIN": 0.3,
      "MAX": 2,
      "DEFAULT": 1,
      "GROUP": "Text"
    },
    {
      "NAME": "bgColor",
      "LABEL": "Background",
      "TYPE": "color",
      "DEFAULT": [
        0,
        0,
        0,
        1
      ],
      "GROUP": "Background"
    },
    {
      "NAME": "transparentBg",
      "LABEL": "Transparent",
      "TYPE": "bool",
      "DEFAULT": true,
      "GROUP": "Background"
    }
  ]
}*/

// ---- universal color block (defaults = no-op) ----
vec3 ucApply(vec3 uc) {
    float ucL = dot(uc, vec3(0.299, 0.587, 0.114));
    uc = mix(vec3(ucL), uc, colorBoost);                      // saturation
    if (hueShift > 0.0005) {                                  // cheap hue rotate (YIQ)
        float hA = hueShift * 6.2831853;
        float hC = cos(hA), hS = sin(hA);
        mat3 hM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
                + hC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
                + hS * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
        uc = clamp(hM * uc, 0.0, 1.0);
    }
    return uc;
}


const float PI = 3.14159265;
const float TWO_PI = 6.28318530;

// Atlas-only font engine (no bitmap fallback — faster ANGLE compile)
float charPixel(int ch, float col, float row) {
    if (ch < 0 || ch > 36) return 0.0;
    vec2 uv = vec2(col / 5.0, row / 7.0);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    return smoothstep(0.1, 0.55, texture2D(fontAtlasTex, vec2((float(ch) + uv.x) / 37.0, uv.y)).r);
}

int getChar(int slot) {
    if (slot == 0)  return int(msg_0);
    if (slot == 1)  return int(msg_1);
    if (slot == 2)  return int(msg_2);
    if (slot == 3)  return int(msg_3);
    if (slot == 4)  return int(msg_4);
    if (slot == 5)  return int(msg_5);
    if (slot == 6)  return int(msg_6);
    if (slot == 7)  return int(msg_7);
    if (slot == 8)  return int(msg_8);
    if (slot == 9)  return int(msg_9);
    if (slot == 10) return int(msg_10);
    if (slot == 11) return int(msg_11);
    if (slot == 12) return int(msg_12);
    if (slot == 13) return int(msg_13);
    if (slot == 14) return int(msg_14);
    if (slot == 15) return int(msg_15);
    if (slot == 16) return int(msg_16);
    if (slot == 17) return int(msg_17);
    if (slot == 18) return int(msg_18);
    if (slot == 19) return int(msg_19);
    if (slot == 20) return int(msg_20);
    if (slot == 21) return int(msg_21);
    if (slot == 22) return int(msg_22);
    if (slot == 23) return int(msg_23);
    if (slot == 24) return int(msg_24);
    if (slot == 25) return int(msg_25);
    if (slot == 26) return int(msg_26);
    if (slot == 27) return int(msg_27);
    if (slot == 28) return int(msg_28);
    if (slot == 29) return int(msg_29);
    if (slot == 30) return int(msg_30);
    if (slot == 31) return int(msg_31);
    if (slot == 32) return int(msg_32);
    if (slot == 33) return int(msg_33);
    if (slot == 34) return int(msg_34);
    if (slot == 35) return int(msg_35);
    if (slot == 36) return int(msg_36);
    if (slot == 37) return int(msg_37);
    if (slot == 38) return int(msg_38);
    if (slot == 39) return int(msg_39);
    if (slot == 40) return int(msg_40);
    if (slot == 41) return int(msg_41);
    if (slot == 42) return int(msg_42);
    if (slot == 43) return int(msg_43);
    if (slot == 44) return int(msg_44);
    if (slot == 45) return int(msg_45);
    if (slot == 46) return int(msg_46);
    return int(msg_47);
}

int charCount() {
    int n = int(msg_len);
    return n > 0 ? n : 1;
}

float sampleChar(int ch, vec2 uv) {
    if (ch < 0 || ch > 36) return 0.0;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    return texture2D(fontAtlasTex, vec2((float(ch) + uv.x) / 37.0, uv.y)).r;
}

float hash(float n) { return fract(sin(n * 127.1) * 43758.5453); }

// =======================================================================
// EFFECT: COIL - text on spiral rings
// =======================================================================

vec4 effectCoil(vec2 uv, int sub) {
    float aspect = RENDERSIZE.x / RENDERSIZE.y;
    int numChars = charCount();

    // Soft-knee audio conditioning (playbook standard snippet).
    float bassP = pow(clamp(smoothstep(0.05, 0.85, audioBass), 0.0, 1.0), 1.6);
    float midP  = pow(clamp(smoothstep(0.08, 0.85, audioMid),  0.0, 1.0), 1.3);
    float highP = pow(clamp(smoothstep(0.10, 0.90, audioHigh), 0.0, 1.0), 1.2);
    float drive = 0.25 + 0.75 * clamp(smoothstep(0.05, 0.9, audioEnergy), 0.0, 1.0);
    float kick  = audioBeatPulse * audioBeatPulse;
    // LINEAR followers (ambient fix r2): the bands are pre-smoothed, so no
    // knee at all — ambient's 0.1-0.8 band swells pass through 1:1.
    float bassSm = clamp(audioBass, 0.0, 1.0);
    float midSm  = clamp(audioMid,  0.0, 1.0);
    float rings = mix(3.0, 15.0, density);
    float charSpacing = mix(0.5, 3.0, intensity);

    float innerR = 0.1, ringGap = 0.06;
    int shapeType = 0;
    bool doPulse = false;

    if (sub == 1) { innerR = 0.08; ringGap = 0.05; shapeType = 1; }
    else if (sub == 2) { innerR = 0.08; ringGap = 0.05; shapeType = 3; }
    else if (sub == 3) { doPulse = true; }

    float eRG = ringGap;
    if (doPulse) eRG *= 1.0 + 0.3*sin(TIME*speed*2.0);
    eRG *= textScale;
    innerR *= textScale;

    vec2 center = vec2(0.5*aspect, 0.5);
    vec2 p = vec2(uv.x*aspect, uv.y) - center;
    p /= 1.0 + 0.28 * bassSm; // bass breathes the whole coil (deep smoothed zoom)
    float radius = length(p);
    // Mid swells swing the whole coil by up to ~26 deg (bounded, smooth,
    // eases back with the envelope) — continuous rotational follow.
    float angOff = 0.45 * midSm;
    float angle = atan(p.y, p.x) - TIME*speed + angOff;
    angle = mod(angle + PI, TWO_PI) - PI;

    float eR = radius;
    if (shapeType == 1) eR = radius / (1.0 + 0.3*cos(5.0*angle));
    else if (shapeType == 3) eR = radius / (0.3 + 0.7*sqrt(abs(cos(2.0*angle))));

    float ringIdx = floor((eR - innerR) / eRG);
    if (ringIdx < 0.0 || ringIdx >= rings) {
        return transparentBg ? vec4(0.0) : bgColor;
    }

    float rcR = innerR + (ringIdx + 0.5)*eRG;
    float cH = eRG*0.75, cW = cH*(5.0/7.0);
    float gW = cW*0.3*charSpacing;
    float cellArc = cW + gW;
    float circ = TWO_PI * rcR;
    float tLen = float(numChars);
    float reps = max(1.0, floor(circ/cellArc/tLen));
    float tca = reps * tLen;
    float aca = circ / tca;
    float acW = aca * (cW/cellArc);

    float na = mod(angle + PI + ringIdx*0.7, TWO_PI);
    float ap = (na/TWO_PI)*tca;
    float ci = floor(ap);
    int ti = int(mod(ci, tLen));

    float ca2 = ((ci+0.5)/tca)*TWO_PI - PI - ringIdx*0.7 + TIME*speed - angOff;
    float ca = cos(ca2), sa = sin(ca2);

    float car = rcR;
    if (shapeType == 1) car = rcR*(1.0+0.3*cos(5.0*ca2));
    else if (shapeType == 3) car = rcR*(0.3+0.7*sqrt(abs(cos(2.0*ca2))));

    vec2 cc = vec2(ca, sa)*car;
    vec2 po = p - cc;
    vec2 lp = vec2(dot(po, vec2(-sa, ca)), dot(po, vec2(ca, sa)));
    float swell = 1.0 + 0.04 * (drive - 0.25) + 0.12 * midP; // energy+mids thicken glyphs in place
    vec2 cellUV = vec2(lp.x/(acW*swell) + 0.5, 1.0 - (lp.y/(cH*swell) + 0.5));

    float textHit = 0.0;
    if (cellUV.x >= 0.0 && cellUV.x <= 1.0 && cellUV.y >= 0.0 && cellUV.y <= 1.0) {
        int ch = getChar(ti);
        if (ch >= 0 && ch <= 36 && ch != 26) textHit = sampleChar(ch, cellUV);
    }

    bool inv = mod(ringIdx, 2.0) < 1.0;
    vec3 fg = inv ? textColor.rgb : bgColor.rgb;
    vec3 bg = inv ? bgColor.rgb : textColor.rgb;
    vec3 fc = mix(bg, fg, textHit);
    float a = 1.0;
    if (transparentBg) { a = textHit; fc = textColor.rgb; }
    // Bass/mid darken-dip on the glyphs (default text is pure white — a
    // multiplicative GAIN clips invisibly there, a dip always reads) plus
    // the original highs/beats sparkle on top.
    fc *= 1.0 - (0.20*bassSm + 0.12*midSm) * textHit;
    fc *= 1.0 + (0.30*highP + 0.30*kick) * textHit;
    return vec4(fc, a);
}

// =======================================================================
// MAIN
// =======================================================================

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE.xy;
    int p = int(preset);
    vec4 col = effectCoil(uv, p);

    if (_voiceGlitch > 0.01) {
        float g = _voiceGlitch;
        float t = TIME * 17.0;
        float band = floor(uv.y * mix(8.0, 40.0, g) + t * 3.0);
        float bandNoise = fract(sin(band * 91.7 + t) * 43758.5);
        float bandActive = step(1.0 - g * 0.6, bandNoise);
        float shift = (bandNoise - 0.5) * 0.08 * g * bandActive;
        float chromaAmt = g * 0.015;
        vec2 uvR = uv + vec2(shift + chromaAmt, 0.0);
        vec2 uvB = uv + vec2(shift - chromaAmt, 0.0);
        vec2 uvG = uv + vec2(shift, chromaAmt * 0.5);
        vec4 cR = effectCoil(uvR, p);
        vec4 cG = effectCoil(uvG, p);
        vec4 cB = effectCoil(uvB, p);
        vec4 glitched = vec4(cR.r, cG.g, cB.b, max(max(cR.a, cG.a), cB.a));
        float scanline = 0.95 + 0.05 * sin(uv.y * RENDERSIZE.y * 1.5 + t * 40.0);
        float blockX = floor(uv.x * 6.0);
        float blockY = floor(uv.y * 4.0);
        float blockNoise = fract(sin((blockX + blockY * 7.0) * 113.1 + floor(t * 8.0)) * 43758.5);
        float dropout = step(1.0 - g * 0.15, blockNoise);
        glitched.rgb *= scanline;
        glitched.rgb *= 1.0 - dropout;
        col = mix(col, glitched, smoothstep(0.0, 0.3, g));
    }

    gl_FragColor = vec4(ucApply(col.rgb), col.a);
}
