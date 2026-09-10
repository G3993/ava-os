/*{
  "CATEGORIES": [
    "Generator",
    "Text"
  ],
  "DESCRIPTION": "Text on a waving flag surface with 3D shading and presets",
  "INPUTS": [
    {
      "NAME": "preset",
      "TYPE": "long",
      "VALUES": [
        0,
        1,
        2,
        3,
        4,
        5,
        6,
        7
      ],
      "LABELS": [
        "Banner",
        "Twist",
        "Flat Sea",
        "Barber",
        "Origami",
        "Cola Waves",
        "B&W",
        "Newsprint"
      ],
      "DEFAULT": 0,
      "LABEL": "Preset"
    },
    {
      "NAME": "waveSize",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.5,
      "GROUP": "Shape / Geometry",
      "LABEL": "Wave Size"
    },
    {
      "NAME": "rowCount",
      "TYPE": "float",
      "MIN": 4,
      "MAX": 20,
      "DEFAULT": 10,
      "GROUP": "Shape / Geometry",
      "LABEL": "Row Count"
    },
    {
      "NAME": "speed",
      "TYPE": "float",
      "MIN": 0.1,
      "MAX": 3,
      "DEFAULT": 0.5,
      "GROUP": "Motion / Animation",
      "LABEL": "Speed"
    },
    {
      "NAME": "textColor",
      "TYPE": "color",
      "DEFAULT": [
        1,
        1,
        1,
        1
      ],
      "GROUP": "Color",
      "LABEL": "Text Color"
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
      "DEFAULT": "ETHEREA",
      "MAX_LENGTH": 12,
      "GROUP": "Text",
      "LABEL": "Message"
    },
    {
      "NAME": "textScale",
      "TYPE": "float",
      "MIN": 0.3,
      "MAX": 2,
      "DEFAULT": 1,
      "GROUP": "Text",
      "LABEL": "Text Scale"
    },
    {
      "NAME": "bgColor",
      "TYPE": "color",
      "DEFAULT": [
        0,
        0,
        0,
        1
      ],
      "GROUP": "Background",
      "LABEL": "Background"
    },
    {
      "NAME": "transparentBg",
      "TYPE": "bool",
      "DEFAULT": true,
      "GROUP": "Background",
      "LABEL": "Transparent Background"
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

    // ── Soft-knee audio conditioning (playbook standard snippet) ─────
    float bassP = pow(clamp(smoothstep(0.05, 0.85, audioBass), 0.0, 1.0), 1.6);
    float midP  = pow(clamp(smoothstep(0.08, 0.85, audioMid),  0.0, 1.0), 1.3);
    float highP = pow(clamp(smoothstep(0.10, 0.90, audioHigh), 0.0, 1.0), 1.2);
    float drive = 0.25 + 0.75 * clamp(smoothstep(0.05, 0.9, audioEnergy), 0.0, 1.0);
    float kick  = audioBeatPulse * audioBeatPulse;
    float wS = waveSize * (1.0 + 0.25 * bassP); // bass billows the flag

    // ── Preset parameters ────────────────────────────────────────────
    float waveAmp = 1.0;      // vertical wave amplitude
    float waveFreq = 3.0;     // how many wave peaks across the flag
    float xShiftAmp = 1.0;    // horizontal displacement strength
    float rowMult = 1.0;      // multiplier on rowCount
    float shadeMult = 1.0;    // shading contrast
    float anchorAmt = 0.5;    // 0 = free, 1 = anchored at left edge
    bool sharpFold = false;
    float extraFreq = 0.0;    // secondary wave frequency

    if (presetIdx == 0) {
        // Banner: classic waving flag, anchored left
        waveAmp = 1.0; waveFreq = 3.0; xShiftAmp = 0.8; anchorAmt = 0.7;
    } else if (presetIdx == 1) {
        // Twist: strong twist, both axes
        waveAmp = 1.3; waveFreq = 2.0; xShiftAmp = 1.5; anchorAmt = 0.0;
        extraFreq = 1.5;
    } else if (presetIdx == 2) {
        // Flat Sea: gentle calm waves
        waveAmp = 0.3; waveFreq = 2.0; xShiftAmp = 0.3; anchorAmt = 0.0;
    } else if (presetIdx == 3) {
        // Barber: diagonal folds
        waveAmp = 1.0; waveFreq = 4.0; xShiftAmp = 1.2; anchorAmt = 0.0;
    } else if (presetIdx == 4) {
        // Origami: sharp angular folds
        sharpFold = true;
        waveAmp = 1.2; waveFreq = 4.0; xShiftAmp = 0.5; anchorAmt = 0.3;
    } else if (presetIdx == 5) {
        // Cola Waves: overlapping frequencies
        waveAmp = 0.8; waveFreq = 3.0; xShiftAmp = 0.7; anchorAmt = 0.0;
        extraFreq = 2.3;
    } else if (presetIdx == 6) {
        // B&W: bold, high contrast
        waveAmp = 1.0; waveFreq = 3.0; xShiftAmp = 0.8; anchorAmt = 0.5;
        rowMult = 1.5; shadeMult = 1.8;
    } else if (presetIdx == 7) {
        // Newsprint: dense, subtle
        waveAmp = 0.2; waveFreq = 2.0; xShiftAmp = 0.2; anchorAmt = 0.0;
        rowMult = 2.0;
    }

    float rows = floor(rowCount * rowMult);

    // ── Flag surface: warp UV through a sine wave field ──────────────
    // The flag waves vertically (Y displacement) as a function of X.
    // Amplitude increases from left to right (anchored pole effect).
    float flagX = uv.x;
    float flagY = uv.y;

    // Envelope: how much wave at this X position (more toward right)
    float envelope = mix(1.0, flagX, anchorAmt);
    envelope = clamp(envelope, 0.0, 1.0);

    // Primary wave: Y displacement based on X position (energy paces the wind)
    float t = TIME * speed * (2.1 + 1.0 * drive);
    float wavePhase = flagX * waveFreq * 6.2832 - t;
    float yWave;
    if (sharpFold) {
        yWave = abs(sin(wavePhase)) * 2.0 - 1.0;
    } else {
        yWave = sin(wavePhase);
    }

    // Secondary wave for complexity
    if (extraFreq > 0.1) {
        yWave += sin(flagX * extraFreq * waveFreq * 6.2832 - t * 1.3) * 0.4;
    }

    // Add a cross-wave (slight Y-dependent wave for diagonal folds)
    if (presetIdx == 3) {
        yWave += sin(flagY * 4.0 * 6.2832 + t * 0.7) * 0.3;
    }

    float yDisp = yWave * wS * waveAmp * 0.15 * envelope;

    // Warp the Y coordinate
    float warpedY = flagY + yDisp;

    // X displacement based on wave derivative (horizontal ripple; mids add turbulence)
    float dWave = cos(wavePhase) * waveFreq * 6.2832;
    float xDisp = dWave * wS * xShiftAmp * 0.02 * envelope * (1.0 + 0.35 * midP);
    float warpedX = flagX + xDisp;

    // ── Determine row from warped Y ──────────────────────────────────
    float rowH = 1.0 / rows;
    float rowIdx = floor(warpedY / rowH);
    rowIdx = clamp(rowIdx, 0.0, rows - 1.0);
    float localY = fract(warpedY / rowH);

    // ── 3D shading from wave surface normal ──────────────────────────
    // The derivative of Y displacement w.r.t. X gives the surface tilt
    float surfSlope = dWave * wS * waveAmp * 0.15 * envelope;
    // Light from the right: shade based on slope
    float shade = 0.5 + 0.5 * (surfSlope / (abs(surfSlope) + 0.3));
    shade = clamp(shade * shadeMult, 0.08, 1.0);

    // ── Character layout ─────────────────────────────────────────────
    float charH = rowH;
    float charW = charH * (5.0 / 7.0) * (1.0 / aspect) * textScale;
    float gapW = charW * 0.15;
    float wordW = float(numChars) * (charW + gapW);

    // Tile text horizontally on warped X
    float posInWord = mod(warpedX, wordW);
    if (posInWord < 0.0) posInWord += wordW;

    float charStep = charW + gapW;
    float charSlotF = posInWord / charStep;
    int charSlot = int(floor(charSlotF));

    float cellLocalX = fract(charSlotF);
    float charFrac = charW / charStep;

    float textHit = 0.0;

    if (cellLocalX < charFrac && charSlot >= 0 && charSlot < numChars) {
        float glyphX = cellLocalX / charFrac;
        float glyphY = localY;

        float gcol = glyphX * 5.0;
        float grow = glyphY * 7.0;

        if (gcol >= 0.0 && gcol < 5.0 && grow >= 0.0 && grow < 7.0) {
            int ch = -1;
            if (charSlot == 0) ch = getChar(0);
            else if (charSlot == 1) ch = getChar(1);
            else if (charSlot == 2) ch = getChar(2);
            else if (charSlot == 3) ch = getChar(3);
            else if (charSlot == 4) ch = getChar(4);
            else if (charSlot == 5) ch = getChar(5);
            else if (charSlot == 6) ch = getChar(6);
            else if (charSlot == 7) ch = getChar(7);
            else if (charSlot == 8) ch = getChar(8);
            else if (charSlot == 9) ch = getChar(9);
            else if (charSlot == 10) ch = getChar(10);
            else if (charSlot == 11) ch = getChar(11);

            if (ch >= 0 && ch <= 25) {
                textHit = charPixel(ch, gcol, grow);
            }
        }
    }

    // ── Alternating row colors ───────────────────────────────────────
    bool inverted = mod(rowIdx, 2.0) < 1.0;

    vec3 fg, bg;
    if (inverted) {
        fg = bgColor.rgb;
        bg = textColor.rgb;
    } else {
        fg = textColor.rgb;
        bg = bgColor.rgb;
    }

    // Apply 3D shading
    vec3 finalCol = mix(bg * shade, fg * shade, textHit);
    float alpha = 1.0;

    if (transparentBg) {
        alpha = textHit;
        finalCol = textColor.rgb * shade;
    }

    // Highs + beat pulse sparkle the glyph pixels only
    finalCol *= 1.0 + (0.30 * highP + 0.30 * kick) * textHit;

    // ---- universal color block (defaults = no-op) ----
    // (background handled by the existing bgColor/transparentBg inputs)
    vec3 uc = finalCol;
    float ucL = dot(uc, vec3(0.299, 0.587, 0.114));
    uc = mix(vec3(ucL), uc, colorBoost);                   // saturation
    if (hueShift > 0.0005) {                               // cheap hue rotate (YIQ)
        float hA = hueShift * 6.2831853;
        float hC = cos(hA), hS = sin(hA);
        mat3 hM = mat3(0.299,0.587,0.114, 0.299,0.587,0.114, 0.299,0.587,0.114)
                + hC * mat3(0.701,-0.587,-0.114, -0.299,0.413,-0.114, -0.300,-0.588,0.886)
                + hS * mat3(0.168,0.330,-0.497, -0.328,0.035,0.292, 1.250,-1.050,-0.203);
        uc = clamp(hM * uc, 0.0, 1.0);
    }

    gl_FragColor = vec4(uc, alpha);
}
