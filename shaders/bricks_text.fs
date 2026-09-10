/*{
  "CATEGORIES": [
    "Generator",
    "Text"
  ],
  "DESCRIPTION": "Screen-filling grid of text with animated per-character wave displacement",
  "INPUTS": [
    {
      "NAME": "preset",
      "LABEL": "Layout Preset",
      "TYPE": "long",
      "VALUES": [
        0,
        1,
        2,
        3,
        4,
        5
      ],
      "LABELS": [
        "Stacks",
        "Bricks",
        "Simple Z",
        "Complex Z",
        "Zebra",
        "Harlequin"
      ],
      "DEFAULT": 0,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "columns",
      "LABEL": "Columns",
      "TYPE": "float",
      "MIN": 5,
      "MAX": 40,
      "DEFAULT": 21,
      "GROUP": "Shape / Geometry"
    },
    {
      "NAME": "rows",
      "LABEL": "Rows",
      "TYPE": "float",
      "MIN": 5,
      "MAX": 30,
      "DEFAULT": 12,
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
      "NAME": "waveAmount",
      "LABEL": "Wave Amount",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.5,
      "GROUP": "Motion / Animation"
    },
    {
      "NAME": "textColor",
      "LABEL": "Text Color",
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
      "NAME": "msg",
      "LABEL": "Message",
      "TYPE": "text",
      "DEFAULT": "ETHEREA",
      "MAX_LENGTH": 12,
      "GROUP": "Text"
    },
    {
      "NAME": "textScale",
      "LABEL": "Text Scale",
      "TYPE": "float",
      "MIN": 0.3,
      "MAX": 2,
      "DEFAULT": 1,
      "GROUP": "Text"
    },
    {
      "NAME": "bgColor",
      "LABEL": "Background Color",
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
      "LABEL": "Transparent Background",
      "TYPE": "bool",
      "DEFAULT": true,
      "GROUP": "Background"
    },
    {
      "NAME": "audioReact",
      "LABEL": "Audio React",
      "TYPE": "float",
      "DEFAULT": 0.35,
      "MIN": 0,
      "MAX": 2,
      "GROUP": "Audio Reactivity"
    }
  ]
}*/

// ── Font engine ──────────────────────────────────────────────────────
// Atlas-based (replaces legacy hardcoded 5x7 packed-bit charData() bitmap
// with a sample from the shared, high-resolution fontAtlasTex).

float charPixel(int ch, float col, float row) {
    if (ch < 0 || ch > 36) return 0.0;
    vec2 uv = vec2(col / 5.0, row / 7.0);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    return smoothstep(0.1, 0.55, texture2D(fontAtlasTex, vec2((float(ch) + uv.x) / 37.0, uv.y)).r);
}

int getChar(int slot) {
    if (slot == 0) return int(msg_0); if (slot == 1) return int(msg_1);
    if (slot == 2) return int(msg_2); if (slot == 3) return int(msg_3);
    if (slot == 4) return int(msg_4); if (slot == 5) return int(msg_5);
    if (slot == 6) return int(msg_6); if (slot == 7) return int(msg_7);
    if (slot == 8) return int(msg_8); if (slot == 9) return int(msg_9);
    if (slot == 10) return int(msg_10); return int(msg_11);
}

int charCount() { int n = int(msg_len); return n > 0 ? n : 1; }

// ── Main ─────────────────────────────────────────────────────────────

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE.xy;
    float aspect = RENDERSIZE.x / RENDERSIZE.y;

    int numChars = charCount();
    int presetIdx = int(preset);

    // Preset parameters
    float waveX = 0.0, waveY = 0.0;
    float freqX = 3.0, freqY = 3.0;
    bool brickOffset = false;
    float phaseMode = 0.0; // 0=col+row, 1=row only, 2=checkerboard
    float extraWaveX = 0.0, extraWaveY = 0.0;
    float extraFreqX = 5.0, extraFreqY = 4.0;

    // Preset 0: Stacks — clean grid, no displacement
    if (presetIdx == 0) {
        waveX = 0.0; waveY = 0.0;
        freqX = 3.0; freqY = 3.0;
        brickOffset = false;
        phaseMode = 0.0;
    }
    // Preset 1: Bricks — brick offset + gentle horizontal wave
    else if (presetIdx == 1) {
        waveX = 0.3; waveY = 0.0;
        freqX = 2.5; freqY = 3.0;
        brickOffset = true;
        phaseMode = 0.0;
    }
    // Preset 2: Simple Z — moderate X and Y wave with diagonal phase
    else if (presetIdx == 2) {
        waveX = 0.5; waveY = 0.5;
        freqX = 3.0; freqY = 3.0;
        brickOffset = false;
        phaseMode = 0.0;
    }
    // Preset 3: Complex Z — stronger overlapping waves with different frequencies
    else if (presetIdx == 3) {
        waveX = 0.8; waveY = 0.6;
        freqX = 3.0; freqY = 2.0;
        brickOffset = false;
        phaseMode = 0.0;
        extraWaveX = 0.4; extraWaveY = 0.3;
        extraFreqX = 5.0; extraFreqY = 4.0;
    }
    // Preset 4: Zebra — strong horizontal wave, phase based on row only
    else if (presetIdx == 4) {
        waveX = 1.0; waveY = 0.0;
        freqX = 4.0; freqY = 3.0;
        brickOffset = false;
        phaseMode = 1.0;
    }
    // Preset 5: Harlequin — diamond pattern, both axes, checkerboard phase
    else {
        waveX = 0.6; waveY = 0.6;
        freqX = 3.0; freqY = 3.0;
        brickOffset = false;
        phaseMode = 2.0;
    }

    // Grid dimensions
    float cols = floor(columns);
    float rws = floor(rows);

    // Cell size in UV space
    float cellW = 1.0 / cols;
    float cellH = 1.0 / rws;

    // Determine grid cell
    float colIdx = floor(uv.x / cellW);
    float rowIdx = floor(uv.y / cellH);
    colIdx = clamp(colIdx, 0.0, cols - 1.0);
    rowIdx = clamp(rowIdx, 0.0, rws - 1.0);

    // Local position within cell [0..1]
    float localX = fract(uv.x / cellW);
    float localY = fract(uv.y / cellH);

    // Brick offset: shift odd rows by half a cell width
    if (brickOffset && mod(rowIdx, 2.0) > 0.5) {
        float shiftedX = uv.x + cellW * 0.5;
        colIdx = floor(shiftedX / cellW);
        colIdx = mod(colIdx, cols);
        localX = fract(shiftedX / cellW);
    }

    // Wave displacement of local position within cell
    float t = TIME * speed * 2.5;

    // Phase calculation based on preset mode
    float phase = colIdx + rowIdx; // default: diagonal
    if (phaseMode > 0.5 && phaseMode < 1.5) {
        // Row only (Zebra)
        phase = rowIdx;
    } else if (phaseMode > 1.5) {
        // Checkerboard (Harlequin)
        phase = (colIdx + rowIdx) * 3.14159;
    }

    float dispX = sin(phase * freqX + t) * waveAmount * waveX * 0.3;
    float dispY = sin(phase * freqY + t * 1.1) * waveAmount * waveY * 0.3;

    // Add secondary wave for Complex Z preset
    if (extraWaveX > 0.0 || extraWaveY > 0.0) {
        float phase2 = colIdx * 0.7 - rowIdx * 1.3;
        dispX += sin(phase2 * extraFreqX + t * 1.3) * waveAmount * extraWaveX * 0.2;
        dispY += sin(phase2 * extraFreqY + t * 0.8) * waveAmount * extraWaveY * 0.2;
    }

    // Apply displacement (shift the local UV, wrapping)
    localX = fract(localX + dispX);
    localY = fract(localY + dispY);

    // Character mapping: which character from the text
    int charIdx = int(mod(colIdx + rowIdx * cols, float(numChars)));

    // ── Audio conditioning — subtle, readability-first. Soft knees,
    //    idle floor = the exact static baseline (all terms zero in silence).
    float bassP = pow(smoothstep(0.05, 0.85, audioBass), 1.6);
    float highP = pow(smoothstep(0.10, 0.90, audioHigh), 1.2);
    float aKick = audioBeatPulse * audioBeatPulse;

    // Render character in cell
    // Character aspect ratio is 5:7
    float charW = 5.0 / 7.0;
    // Bass breathes the glyph weight very slightly — the dominant structure.
    float scaleBreath = 1.0 + audioReact * 0.04 * bassP;
    // Scale character within cell
    float scaleX = textScale * charW * scaleBreath;
    float scaleY = textScale * scaleBreath;
    float marginX = (1.0 - scaleX) * 0.5;
    float marginY = (1.0 - scaleY) * 0.5;
    // Highs add a hair of per-character letterspacing breathing — sparse,
    // phase-locked to each cell so it never reads as a global pump.
    marginX += audioReact * 0.012 * highP * sin(TIME * 3.0 + colIdx * 1.7 + rowIdx * 0.9);

    float textHit = 0.0;

    if (localX >= marginX && localX < 1.0 - marginX && localY >= marginY && localY < 1.0 - marginY) {
        float glyphX = (localX - marginX) / scaleX;
        float glyphY = (localY - marginY) / scaleY;
        float gcol = glyphX * 5.0;
        float grow = glyphY * 7.0;

        if (gcol >= 0.0 && gcol < 5.0 && grow >= 0.0 && grow < 7.0) {
            // Unrolled if/else chain for charIdx 0-11
            int ch = -1;
            int ci = int(mod(float(charIdx), float(numChars)));
            if (ci == 0) ch = getChar(0);
            else if (ci == 1) ch = getChar(1);
            else if (ci == 2) ch = getChar(2);
            else if (ci == 3) ch = getChar(3);
            else if (ci == 4) ch = getChar(4);
            else if (ci == 5) ch = getChar(5);
            else if (ci == 6) ch = getChar(6);
            else if (ci == 7) ch = getChar(7);
            else if (ci == 8) ch = getChar(8);
            else if (ci == 9) ch = getChar(9);
            else if (ci == 10) ch = getChar(10);
            else if (ci == 11) ch = getChar(11);

            if (ch >= 0 && ch <= 25) {
                textHit = charPixel(ch, gcol, grow);
            }
        }
    }

    // Alternating row colors
    bool inverted = mod(rowIdx, 2.0) < 1.0;

    vec3 fg, bg;
    if (inverted) {
        fg = bgColor.rgb;
        bg = textColor.rgb;
    } else {
        fg = textColor.rgb;
        bg = bgColor.rgb;
    }

    vec3 finalCol = mix(bg, fg, textHit);
    float alpha = 1.0;

    if (transparentBg) {
        alpha = textHit;
        finalCol = textColor.rgb;
    }

    // Glow breathes with the music: a continuous (level-driven) warm lean
    // plus a beat pulse on top — decaying accent, not a full-frame flash.
    // Multiplicative tint (not additive) so it reads even on saturated
    // white text, where additive glow would just clip invisibly.
    vec3 beatTint = vec3(1.0, 0.65, 0.35);
    float glowAmt = clamp(audioReact * (0.35 * highP + 0.9 * aKick), 0.0, 1.0);
    finalCol = mix(finalCol, finalCol * beatTint, glowAmt);

    // Surprise: every ~28s a single brick falls — for ~0.5s the lower
    // edge sags, then snaps back. Lego physics rebellion.
    {
        vec2 _suv = gl_FragCoord.xy / RENDERSIZE;
        float _ph = fract(TIME / 28.0);
        float _f  = smoothstep(0.0, 0.04, _ph) * smoothstep(0.18, 0.10, _ph);
        if (_suv.y < 0.15) {
            float _drop = exp(-pow((_suv.x - 0.5) * 4.0, 2.0)) * 0.04;
            finalCol = mix(finalCol, finalCol * 0.7, _f * _drop * 30.0);
        }
    }

    // ---- universal color block (defaults = no-op) ----
    vec3 uc = finalCol;
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
    finalCol = uc;

    gl_FragColor = vec4(finalCol, alpha);
}
