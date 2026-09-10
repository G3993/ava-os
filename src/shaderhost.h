// ShaderClaw3 library host — renders the ISF-dialect audio-reactive shaders
// (JSON header + GLSL, PASSINDEX multipass with persistent targets) as the
// AVA OS background, following the same uniform contract as the
// ShaderClaw3 web host / Easel: TIME, RENDERSIZE, PASSINDEX, FRAMEINDEX,
// isf_FragNormCoord, audioFFT + audioLevel/Bass/Mid/High, mouse*, INPUTS.
#pragma once
#include <string>
#include <vector>
#include "glplat.h"

struct ShaderParam {
    enum Type { Float, Bool, Long, Color, Point2D, Image, Event, Text } type = Float;
    std::string name, label, group;
    float def[4] = {0, 0, 0, 1}, cur[4] = {0, 0, 0, 1};
    float minV = 0, maxV = 1;
    std::vector<int> values;            // long
    std::vector<std::string> labels;    // long
    std::string text;                   // text
    int maxLen = 12;                    // text
};

struct ShaderEntry {
    int id = 0;
    std::string title, file, description;
    bool hidden = false;
};

struct AudioUniforms {
    float level = 0, bass = 0, mid = 0, high = 0;
    float sub = 0, lowMid = 0, onset = 0, bpm = 120;
    const float* spectrum = nullptr; // 256 bins or null
};

class ShaderHost {
public:
    bool loadLibrary(const std::string& dir); // dir with manifest.json + *.fs
    const std::vector<ShaderEntry>& entries() const { return entries_; }

    bool load(int entryIndex);   // compile + build pass chain
    void unload();               // back to no shader (black)
    int currentIndex() const { return current_; }
    const char* currentTitle() const;
    std::vector<ShaderParam>& params() { return params_; }

    // render all passes into the output texture (internal resolution)
    void render(float timeSec, const AudioUniforms& au,
                float mouseX, float mouseY, float mouseDown);
    GLuint outputTexture() const { return outTex_; }
    bool active() const { return prog_ != 0; }

    // draw a texture as a fullscreen quad in the CURRENT GL context
    // (creates per-context VAO/program on first use)
    void drawFullscreen(GLuint tex, float dim, float vignette = 0.0f);

    std::string lastError;
    int renderW = 1280, renderH = 720;

private:
    struct Target {
        std::string name;
        GLuint tex[2] = {0, 0};
        GLuint fbo[2] = {0, 0};
        int front = 0;
        bool persistent = false;
    };
    struct Pass { int targetIdx = -1; }; // -1 → output

    bool compile(const std::string& src);
    void destroyGL();
    void allocTargets();

    std::string libDir_;
    std::vector<ShaderEntry> entries_;
    int current_ = -1;

    GLuint prog_ = 0, vao_ = 0, vbo_ = 0;
    GLuint outTex_ = 0, outFbo_ = 0;
    GLuint fftTex_ = 0, blackTex_ = 0;
    std::vector<Target> targets_;
    std::vector<Pass> passes_;
    std::vector<ShaderParam> params_;
    int frame_ = 0;
    float lastTime_ = 0;
    // band-speed clocks for the audio bus (audioBassTime etc.)
    double bassClk_ = 0, midClk_ = 0, highClk_ = 0, lvlClk_ = 0, beatClk_ = 0;
};
