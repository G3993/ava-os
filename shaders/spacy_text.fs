/*{
  "CATEGORIES": [
    "Generator",
    "Text"
  ],
  "DESCRIPTION": "Rows of text at varying depth scales creating a perspective tunnel effect",
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
        6
      ],
      "LABELS": [
        "Post Space",
        "Bridge",
        "Beach",
        "Moon",
        "X",
        "Whitney",
        "Recede"
      ],
      "DEFAULT": 0,
      "LABEL": "Preset"
    },
    {
      "NAME": "rowCount",
      "TYPE": "float",
      "MIN": 3,
      "MAX": 20,
      "DEFAULT": 7,
      "GROUP": "Shape / Geometry",
      "LABEL": "Row Count"
    },
    {
      "NAME": "speed",
      "TYPE": "float",
      "MIN": 0,
      "MAX": 3,
      "DEFAULT": 0.3,
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
      "GROUP": "Background"
    },
    {
      "NAME": "transparentBg",
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

// ── Secondary detail layer (cosmic dust) ────────────────────────────
// Cheap single-cell hash sparkle field — no loops, no new samplers.
float hashDust(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

// ── Main ─────────────────────────────────────────────────────────────

void main() {
    vec2 uv = gl_FragCoord.xy / RENDERSIZE.xy;
    float aspect = RENDERSIZE.x / RENDERSIZE.y;
    int numChars = charCount();
    int presetIdx = int(preset);

    // Preset parameters
    float minScale = 0.3;
    float maxScale = 2.5;
    float tracking = 0.15;
    float scrollMult = 1.0;
    bool mirror = false;

    // Preset 0: Post Space — moderate scale range, clean depth
    if (presetIdx == 0) {
        minScale = 0.3;
        maxScale = 2.5;
        tracking = 0.15;
        scrollMult = 1.0;
        mirror = false;
    }
    // Preset 1: Bridge — tight spacing, wide scale range, faster scroll
    if (presetIdx == 1) {
        minScale = 0.2;
        maxScale = 3.0;
        tracking = 0.05;
        scrollMult = 1.4;
        mirror = false;
    }
    // Preset 2: Beach — gentle scale, generous spacing, slow scroll
    if (presetIdx == 2) {
        minScale = 0.5;
        maxScale = 1.8;
        tracking = 0.25;
        scrollMult = 0.6;
        mirror = false;
    }
    // Preset 3: Moon — large bold text, wide scale range
    if (presetIdx == 3) {
        minScale = 0.4;
        maxScale = 3.5;
        tracking = 0.15;
        scrollMult = 0.8;
        mirror = false;
    }
    // Preset 4: X — extreme scale contrast, dramatic depth
    if (presetIdx == 4) {
        minScale = 0.1;
        maxScale = 4.0;
        tracking = 0.1;
        scrollMult = 1.2;
        mirror = false;
    }
    // Preset 5: Whitney — smooth scale, generous spacing, elegant
    if (presetIdx == 5) {
        minScale = 0.4;
        maxScale = 2.0;
        tracking = 0.2;
        scrollMult = 0.9;
        mirror = true;
    }
    // Preset 6: Recede — very small center text, strong depth
    if (presetIdx == 6) {
        minScale = 0.15;
        maxScale = 2.0;
        tracking = 0.12;
        scrollMult = 1.0;
        mirror = false;
    }

    float rws = floor(rowCount);
    float rowH = 1.0 / rws;

    // ── Audio conditioning (soft knees, idle floor — playbook routing) ──
    float driveKnee = smoothstep(0.05, 0.85, audioEnergy);
    float bassP = pow(smoothstep(0.05, 0.85, audioBass), 1.6);
    float highP = pow(smoothstep(0.10, 0.90, audioHigh), 1.2);
    float beatP = audioBeatPulse * audioBeatPulse;

    // Audio time-warp clock: rows keep drifting on their own in total
    // silence (idle floor), energy speeds the whole tunnel up on top —
    // the scene never freezes and never snaps in lockstep with a beat.
    float musicTime = TIME * (0.35 + 1.35 * driveKnee);

    // Vertical scroll
    float scrollY = uv.y + musicTime * speed * scrollMult;

    // Wrap into [0..1] repeating band
    float warpedY = mod(scrollY, 1.0);

    // Which row?
    float rowIdx = floor(warpedY / rowH);
    rowIdx = clamp(rowIdx, 0.0, rws - 1.0);

    // Local Y within this row [0..1]
    float localY = fract(warpedY / rowH);

    // Scale per row: distance from center of the row stack
    // rowNorm = 0 at bottom row, 1 at top row
    float rowNorm = (rowIdx + 0.5) / rws;
    float distFromCenter = abs(rowNorm - 0.5) * 2.0; // 0 at center, 1 at edges

    // Smooth the distance curve for a more natural perspective feel
    float scaleCurve = distFromCenter * distFromCenter; // quadratic falloff
    float rowScale = mix(minScale, maxScale, scaleCurve) * textScale;
    rowScale *= 1.0 + 0.20 * bassP; // bass adds structural weight (±20%)

    // Character dimensions at this row's scale
    // charH is the fraction of the row this text occupies
    float charH = rowH * rowScale;
    float charW = charH * (5.0 / 7.0) * (1.0 / aspect);
    float gapW = charW * tracking;

    // Total width of one copy of the message
    float wordW = float(numChars) * (charW + gapW);
    if (wordW < 0.001) wordW = 0.001; // prevent division by zero

    // Horizontal position — mirror bottom half if enabled
    float px = uv.x;
    if (mirror && rowNorm < 0.5) {
        px = 1.0 - px;
    }

    // Tile/repeat the text horizontally
    float posInWord = mod(px, wordW);
    if (posInWord < 0.0) posInWord += wordW;

    // Which character slot?
    float charStep = charW + gapW;
    float charSlotF = posInWord / charStep;
    int charSlot = int(floor(charSlotF));

    // Position within the character cell
    float cellLocalX = fract(charSlotF);
    float charFrac = charW / charStep;

    // Vertical: center the text in the row based on scale
    float textStartY = 0.5 - rowScale * 0.5;
    float glyphY = (localY - textStartY) / rowScale;

    float textHit = 0.0;

    if (cellLocalX < charFrac && charSlot >= 0 && charSlot < numChars && glyphY >= 0.0 && glyphY <= 1.0) {
        float glyphX = cellLocalX / charFrac;

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

    // Alternating color inversion per row
    bool inverted = mod(rowIdx, 2.0) < 1.0;

    vec3 fg, bg;
    if (inverted) {
        fg = bgColor.rgb;
        bg = textColor.rgb;
    } else {
        fg = textColor.rgb;
        bg = bgColor.rgb;
    }

    // Beat flash — decaying brightness lift on the glyph only (event-driven,
    // kept well under the ≤30% depth budget; never touches the background).
    fg += fg * beatP * 0.22;

    // ── Cosmic dust: sparse hash-cell sparkle behind the text ──────────
    // Gives the frame real secondary structure instead of a flat field,
    // fits the tunnel/"spacy" identity, and breathes on its own (musicTime)
    // while audioHigh gates a sparse ±40% swing on top.
    vec2 dustUv = uv * vec2(26.0 * aspect, 26.0) + vec2(musicTime * 0.02, 0.0);
    vec2 dustCell = floor(dustUv);
    vec2 dustF = fract(dustUv);
    float dh = hashDust(dustCell);
    float present = step(0.86, dh); // ~14% of cells host a mote
    vec2 dustPos = vec2(hashDust(dustCell + 1.7), hashDust(dustCell + 5.1));
    float dustD = length(dustF - dustPos);
    float dustR = mix(0.03, 0.10, hashDust(dustCell + 3.3));
    float mote = smoothstep(dustR, 0.0, dustD) * present;
    float twinkle = 0.5 + 0.5 * sin(musicTime * 2.4 + dh * 6.2831853);
    float sparkle = mote * twinkle * (0.35 + 0.65 * highP) * (1.0 - textHit);
    vec3 dustCol = mix(bgColor.rgb, textColor.rgb, 0.65); // designed accent, no hue cycling

    vec3 finalCol = mix(bg, fg, textHit) + dustCol * sparkle;
    float alpha = 1.0;

    if (transparentBg) {
        // Straight alpha for real compositing (alpha = textHit, plus a
        // faint dust contribution), but keep RGB structured — premultiplied
        // glyph + dust — instead of a flat fill, so any RGB-only reader
        // still sees the actual scene rather than a blown-out solid frame.
        alpha = max(textHit, sparkle * 0.55);
        finalCol = fg * textHit + dustCol * sparkle;
    }

    gl_FragColor = vec4(ucApply(finalCol), alpha);
}
