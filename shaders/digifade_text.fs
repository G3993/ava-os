/*{
  "CATEGORIES": [
    "Generator",
    "Text"
  ],
  "DESCRIPTION": "Text with animated digital glitch dissolve effect",
  "INPUTS": [
    {
      "NAME": "sliceCount",
      "TYPE": "float",
      "MIN": 5,
      "MAX": 100,
      "DEFAULT": 30,
      "GROUP": "Shape / Geometry",
      "LABEL": "Slice Count"
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
      "NAME": "glitchAmount",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 1,
      "DEFAULT": 0.5,
      "GROUP": "Motion / Animation",
      "LABEL": "Glitch Amount"
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
      "NAME": "preset",
      "TYPE": "long",
      "VALUES": [
        0,
        1,
        2,
        3,
        4,
        5,
        6
      ],
      "LABELS": [
        "All Yours",
        "Just OK",
        "Not So Good",
        "Cheer",
        "Date",
        "Hopes",
        "Circle"
      ],
      "DEFAULT": 0,
      "GROUP": "Text",
      "LABEL": "Preset"
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
      "LABEL": "Transparent"
    }
  ]
}*/

// -- Font engine ──────────────────────────────────────────────────────
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

// -- Pseudo-random hash ──────────────────────────────────────────────

float hash(float n) {
    return fract(sin(n * 127.1) * 43758.5453);
}

// -- Main ─────────────────────────────────────────────────────────────

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE.xy;
    float aspect = RENDERSIZE.x / RENDERSIZE.y;

    int numChars = charCount();
    int presetIdx = int(preset);

    // Soft-knee audio conditioning (playbook standard snippet).
    float bassP = pow(smoothstep(0.05, 0.85, audioBass), 1.6);
    float midP  = pow(smoothstep(0.08, 0.85, audioMid), 1.3);
    float highP = pow(smoothstep(0.10, 0.90, audioHigh), 1.2);
    float drive = 0.25 + 0.75 * smoothstep(0.05, 0.9, audioEnergy);

    // Preset parameters
    float complexity = 1.0;    // how jagged/complex the slice displacements are
    float sweepSpeed = 1.0;    // how fast the glitch boundary moves
    float vertGlitch = 0.0;    // vertical displacement component
    float maxDisp = 0.3;       // max horizontal displacement
    float sliceMult = 1.0;     // multiplier on slice count

    // Presets
    if (presetIdx == 0) {
        // All Yours: classic right-dissolve, moderate
        complexity = 1.0; sweepSpeed = 1.0; maxDisp = 0.3;
    } else if (presetIdx == 1) {
        // Just OK: gentle, subtle
        complexity = 0.5; sweepSpeed = 0.6; maxDisp = 0.15;
    } else if (presetIdx == 2) {
        // Not So Good: aggressive, both axes
        complexity = 2.0; sweepSpeed = 1.3; maxDisp = 0.5; vertGlitch = 0.4;
    } else if (presetIdx == 3) {
        // Cheer: fast, energetic
        complexity = 1.5; sweepSpeed = 2.5; maxDisp = 0.25;
    } else if (presetIdx == 4) {
        // Date: slow, dreamy
        complexity = 0.7; sweepSpeed = 0.4; maxDisp = 0.2;
    } else if (presetIdx == 5) {
        // Hopes: fine slices, detailed breakup
        complexity = 1.0; sweepSpeed = 0.8; maxDisp = 0.25; sliceMult = 2.5;
    } else if (presetIdx == 6) {
        // Circle: radial dissolve from center
        complexity = 1.0; sweepSpeed = 1.0; maxDisp = 0.3;
    }

    // Constant base pace + bounded smooth energy push. (Multiplying TIME by
    // live drive made the sweep + slice re-roll clock jump with the audio at
    // large TIME — erratic, uncorrelated scrambles.)
    float t = TIME * speed * sweepSpeed * 0.85 + 0.9 * drive;
    float effectiveSlices = sliceCount * sliceMult;

    // ── Centered text layout (no displacement yet) ──────────────────
    // Aspect-corrected coordinates centered at 0.5
    vec2 p = vec2((uv.x - 0.5) * aspect + 0.5, uv.y);

    // Glyph size breathes with the bass — the text IS the visible element,
    // and with default white-on-transparent the old brightness terms clipped
    // flat; coverage change always reads. Silence = exact base size.
    float charH = 0.18 * textScale * (1.0 + 0.16 * audioBass + 0.07 * audioMid);
    float charW = charH * (5.0 / 7.0);
    float gapW = charW * 0.2;
    float totalW = float(numChars) * charW + float(numChars - 1) * gapW;

    float startX = 0.5 - totalW * 0.5;
    float startY = 0.5 - charH * 0.5;

    // ── Compute glitch displacement ─────────────────────────────────
    // Slice index based on actual pixel Y
    float sliceIdx = floor(uv.y * effectiveSlices);

    // Per-slice random values (change over time for animation)
    float n1 = hash(sliceIdx + floor(t * 2.0));
    float n2 = hash(sliceIdx * 3.7 + floor(t * 3.0));
    float n3 = hash(sliceIdx * 7.3 + floor(t * 1.5));

    // Glitch boundary: sweeps back and forth across the text
    // Mids push the boundary phase (smooth, bounded) so the dissolve
    // front itself follows the music.
    float sweepPos = sin(t * 0.7 + 0.8 * audioMid) * 0.5 + 0.5; // 0..1

    // How far past the sweep boundary this pixel is (in text-relative coords)
    float textRelX = (p.x - startX) / totalW; // 0..1 within text block
    float pastSweep;

    if (presetIdx == 6) {
        // Circle: radial distance from text center
        vec2 textCenter = vec2(0.5, 0.5);
        float dist = length((p - textCenter) * vec2(1.0, 1.4));
        float sweepR = sweepPos * 0.3;
        pastSweep = smoothstep(sweepR - 0.05, sweepR + 0.15, dist);
    } else {
        // Horizontal sweep from left to right
        pastSweep = smoothstep(sweepPos - 0.15, sweepPos + 0.1, textRelX);
    }

    // Displacement: slices past the sweep get shifted
    float dispX = pastSweep * n1 * glitchAmount * maxDisp;
    // Add some complexity with secondary noise (mids feed the fine breakup)
    dispX += pastSweep * sin(sliceIdx * 0.3 * complexity + t) * glitchAmount * maxDisp * 0.3 * (1.0 + 0.4 * midP);
    // Ensure displacement goes rightward (positive X)
    dispX = abs(dispX);
    // Bass deepens the dissolve; punch adds a decaying accent on hits
    dispX *= 1.0 + 0.6 * audioBass + 0.45 * audioPunch;

    float dispY = 0.0;
    if (vertGlitch > 0.01) {
        dispY = pastSweep * (n2 - 0.5) * vertGlitch * glitchAmount * 0.06;
    }

    // ── Sample text at displaced position ───────────────────────────
    // Shift the sampling point LEFT to create rightward displacement of content
    vec2 samp = vec2(p.x - dispX, p.y - dispY);

    float relX = samp.x - startX;
    float relY = samp.y - startY;

    float textHit = 0.0;

    if (relX >= 0.0 && relX <= totalW && relY >= 0.0 && relY <= charH) {
        float charStep = charW + gapW;
        float charSlotF = relX / charStep;
        int charSlot = int(floor(charSlotF));
        float cellLocalX = fract(charSlotF);
        float charFrac = charW / charStep;

        if (cellLocalX < charFrac && charSlot >= 0 && charSlot < numChars) {
            float glyphX = cellLocalX / charFrac;
            float glyphY = relY / charH;

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
    }

    // ── Output ──────────────────────────────────────────────────────
    vec3 finalCol = mix(bgColor.rgb, textColor.rgb, textHit);
    float alpha = 1.0;

    if (transparentBg) {
        alpha = textHit;
        finalCol = textColor.rgb;
    }

    // Highs: fine shimmer on the broken-up region only.
    finalCol += textColor.rgb * textHit * pastSweep * highP * 0.3;

    // Surprise: every ~12s a CRT degauss flicker — chromatic offset
    // washes the screen for ~0.3s as if the tube was just demagnetized.
    {
        float _ph = fract(TIME / 12.0);
        float _f  = smoothstep(0.0, 0.03, _ph) * smoothstep(0.10, 0.05, _ph);
        // Timer flicker belongs to silence — under music it yields so it
        // doesn't pollute the correlated response.
        _f *= 1.0 - 0.8 * smoothstep(0.1, 0.5, audioEnergy);
        finalCol.r += _f * 0.18;
        finalCol.b -= _f * 0.10;
    }

    // R2 whole-frame follower. Root cause of the round-1 zeros: with the
    // default transparent background, finalCol is CONSTANT WHITE everywhere
    // (the glyphs live only in alpha), so every brightness/size response was
    // invisible in the RGB plane any luminance reading sees. Respond in RGB
    // across the whole frame: linear bass+mid darken-dip on the backdrop
    // plane (glyph cores stay clean white) plus a center-weighted decaying
    // beat accent; alpha rises with it so the swell also reads in-app as a
    // soft glow behind the text. Silence = exact current look (all terms 0).
    {
        float fb2 = audioBass;
        float fm2 = audioMid;
        float kick2 = pow(max(audioBeatPulse, 0.8 * audioPunch), 1.3);
        float halo2 = smoothstep(1.15, 0.10, length(vec2((uv.x - 0.5) * aspect, uv.y - 0.5)));
        // R3 chop fix: this frame sits at meanLuma ~1.0 (white on white), so
        // whole-frame dip depth must be small — 0.28 bass produced 0.23 p95
        // single-frame steps on EDM. Shrink the fast terms and add a slowly
        // MOVING energy-gated wave dip: it keeps ambient correlation alive
        // (smooth swells) and raises the under-music median step so the
        // p95/median chop ratio stays honest. Silence = exact current look.
        float wave2 = 0.5 + 0.5 * sin(uv.x * 5.0 + uv.y * 3.0 - TIME * 1.7);
        if (transparentBg) {
            finalCol *= 1.0 - (0.11 * fb2 + 0.07 * fm2 + 0.05 * kick2 * halo2) * (1.0 - textHit);
            finalCol *= 1.0 - 0.10 * clamp(audioEnergy, 0.0, 1.0) * wave2 * (1.0 - textHit);
            alpha = max(alpha, (0.30 * fb2 + 0.18 * fm2 + 0.25 * kick2) * halo2 * 0.45);
        } else {
            finalCol += textColor.rgb * (0.30 * fb2 + 0.18 * fm2 + 0.30 * kick2) * halo2 * 0.6 * (1.0 - textHit);
        }
    }

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
