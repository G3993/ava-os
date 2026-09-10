#include "shaderhost.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <regex>
#include <map>
#include <set>
#include <cstdio>

using json = nlohmann::json;

static std::string readFile(const std::string& p) {
    std::ifstream f(p);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool ShaderHost::loadLibrary(const std::string& dir) {
    libDir_ = dir;
    entries_.clear();
    // optional skip list (one .fs filename per line) — shaders verified broken
    std::set<std::string> skip;
    {
        std::ifstream sf(dir + "/skip.txt");
        std::string line;
        while (std::getline(sf, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
                line.pop_back();
            if (!line.empty() && line[0] != '#') skip.insert(line);
        }
    }
    try {
        json m = json::parse(readFile(dir + "/manifest.json"));
        for (auto& e : m) {
            ShaderEntry se;
            if (e.contains("id")) {
                if (e["id"].is_number()) se.id = e["id"].get<int>();
                else se.id = atoi(e["id"].get<std::string>().c_str());
            }
            se.title = e.value("title", "untitled");
            se.file = e.value("file", "");
            se.description = e.value("description", "");
            se.hidden = e.value("hidden", false);
            if (se.hidden || se.file.empty()) continue;
            // only .fs GLSL entries (skip three.js .scene.js scenes)
            if (se.file.size() < 3 || se.file.substr(se.file.size() - 3) != ".fs") continue;
            if (skip.count(se.file)) continue;
            // skip text shaders (message/typography ports) — not wanted in the OS
            {
                std::ifstream f(dir + "/" + se.file);
                std::string head(8192, '\0');
                f.read(&head[0], 8192);
                if (head.find("\"TYPE\": \"text\"") != std::string::npos ||
                    head.find("\"TYPE\":\"text\"") != std::string::npos)
                    continue;
            }
            entries_.push_back(se);
        }
    } catch (std::exception& ex) {
        lastError = std::string("manifest: ") + ex.what();
        return false;
    }
    return !entries_.empty();
}

const char* ShaderHost::currentTitle() const {
    if (current_ < 0 || current_ >= (int)entries_.size()) return "None";
    return entries_[current_].title.c_str();
}

// ── GLSL assembly (mirrors the ShaderClaw3 web host / Easel translation) ──
static const char* kVertexSrc = R"(#version 330 core
const vec2 verts[4] = vec2[4](vec2(-1,-1), vec2(1,-1), vec2(-1,1), vec2(1,1));
out vec2 isf_FragNormCoord;
void main() {
    vec2 p = verts[gl_VertexID];
    isf_FragNormCoord = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0, 1);
}
)";

bool ShaderHost::load(int idx) {
    if (idx < 0 || idx >= (int)entries_.size()) return false;
    lastError.clear();
    std::string src = readFile(libDir_ + "/" + entries_[idx].file);
    if (src.empty()) { lastError = "read failed: " + entries_[idx].file; return false; }

    // extract ISF JSON header
    size_t h0 = src.find("/*");
    size_t h1 = src.find("*/");
    if (h0 == std::string::npos || h1 == std::string::npos) {
        lastError = "no ISF header";
        return false;
    }
    json hdr;
    try {
        hdr = json::parse(src.substr(h0 + 2, h1 - h0 - 2));
    } catch (std::exception& ex) {
        lastError = std::string("header parse: ") + ex.what();
        return false;
    }
    std::string body = src.substr(h1 + 2);

    // inputs
    params_.clear();
    if (hdr.contains("INPUTS")) {
        for (auto& in : hdr["INPUTS"]) {
            ShaderParam p;
            p.name = in.value("NAME", "");
            p.label = in.value("LABEL", p.name);
            p.group = in.value("GROUP", "");
            std::string t = in.value("TYPE", "float");
            auto num = [&](const json& v, float d) -> float {
                return v.is_number() ? v.get<float>() : d;
            };
            if (t == "float") {
                p.type = ShaderParam::Float;
                p.def[0] = in.contains("DEFAULT") ? num(in["DEFAULT"], 0) : 0;
                p.minV = in.contains("MIN") ? num(in["MIN"], 0) : 0;
                p.maxV = in.contains("MAX") ? num(in["MAX"], 1) : 1;
            } else if (t == "bool") {
                p.type = ShaderParam::Bool;
                p.def[0] = (in.contains("DEFAULT") &&
                            ((in["DEFAULT"].is_boolean() && in["DEFAULT"].get<bool>()) ||
                             (in["DEFAULT"].is_number() && in["DEFAULT"].get<float>() > 0.5f)))
                               ? 1.0f : 0.0f;
            } else if (t == "long") {
                p.type = ShaderParam::Long;
                if (in.contains("VALUES"))
                    for (auto& v : in["VALUES"]) p.values.push_back(v.get<int>());
                if (in.contains("LABELS"))
                    for (auto& l : in["LABELS"]) p.labels.push_back(l.get<std::string>());
                p.def[0] = in.contains("DEFAULT") ? num(in["DEFAULT"], 0) : 0;
            } else if (t == "color") {
                p.type = ShaderParam::Color;
                if (in.contains("DEFAULT") && in["DEFAULT"].is_array()) {
                    int i = 0;
                    for (auto& v : in["DEFAULT"]) { if (i < 4) p.def[i++] = v.get<float>(); }
                }
            } else if (t == "point2D") {
                p.type = ShaderParam::Point2D;
                if (in.contains("DEFAULT") && in["DEFAULT"].is_array()) {
                    int i = 0;
                    for (auto& v : in["DEFAULT"]) { if (i < 2) p.def[i++] = v.get<float>(); }
                }
            } else if (t == "image") {
                p.type = ShaderParam::Image;
            } else if (t == "text") {
                p.type = ShaderParam::Text;
                if (in.contains("DEFAULT") && in["DEFAULT"].is_string())
                    p.text = in["DEFAULT"].get<std::string>();
                p.maxLen = in.contains("MAX") && in["MAX"].is_number()
                               ? in["MAX"].get<int>() : 12;
                if (p.maxLen <= 0 || p.maxLen > 128) p.maxLen = 12;
            } else { // event etc.
                p.type = ShaderParam::Event;
            }
            for (int i = 0; i < 4; i++) p.cur[i] = p.def[i];
            params_.push_back(p);
        }
    }

    // passes
    destroyGL();
    targets_.clear();
    passes_.clear();
    std::map<std::string, int> targetIdx;
    if (hdr.contains("PASSES") && hdr["PASSES"].is_array() && hdr["PASSES"].size() > 0) {
        for (auto& ps : hdr["PASSES"]) {
            Pass pass;
            std::string tname = ps.value("TARGET", "");
            if (!tname.empty()) {
                if (!targetIdx.count(tname)) {
                    Target t;
                    t.name = tname;
                    t.persistent = ps.value("PERSISTENT", false);
                    targetIdx[tname] = (int)targets_.size();
                    targets_.push_back(t);
                }
                pass.targetIdx = targetIdx[tname];
            }
            passes_.push_back(pass);
        }
        // if every pass has a target, the last target is the displayed image
    } else {
        passes_.push_back({});
    }

    // ── assemble fragment source ──
    std::ostringstream fs;
    fs << "#version 330 core\n"
       << "out vec4 FragColor;\n"
       << "#define gl_FragColor FragColor\n"
       << "in vec2 isf_FragNormCoord;\n"
       << "uniform float TIME;\nuniform float TIMEDELTA;\n"
       << "uniform vec2 RENDERSIZE;\nuniform int PASSINDEX;\nuniform int FRAMEINDEX;\n"
       << "uniform vec2 mousePos;\nuniform vec2 mouseDelta;\n"
       << "uniform float mouseDown;\nuniform float pinchHold;\n"
       << "uniform sampler2D audioFFT;\n"
       << "uniform float audioLevel;\nuniform float audioBass;\n"
       << "uniform float audioMid;\nuniform float audioHigh;\n"
       << "uniform float msgAge;\n"
       << "uniform sampler2D varFontTex;\nuniform sampler2D fontAtlasTex;\n"
       << "uniform float useFontAtlas;\nuniform float _voiceGlitch;\nuniform float _voiceLevel;\n"
       << "uniform sampler2D mpHandLandmarks;\nuniform sampler2D mpFaceLandmarks;\n"
       << "uniform sampler2D mpPoseLandmarks;\nuniform sampler2D mpSegMask;\n"
       << "uniform sampler2D mpSegmentation;\n"
       << "uniform float mpHandCount;\nuniform vec3 mpHandPos;\nuniform vec3 mpHandPos2;\n"
       << "uniform float _transparentBg;\n"
       // ── Audio Feature Bus (Easel contract, all tiers, always declared) ──
       << "uniform float audioSub, audioLowMid, audioHighMid, audioTreble, audioPunch;\n"
       << "uniform float audioBeat, audioBeatPhase, audioBeatPulse, audioBarPhase, audioBPM, audioTempo01;\n"
       << "uniform float audioBrightness, audioSpread, audioRolloff, audioFlatness, audioTexture;\n"
       << "uniform float audioFlux, audioOnset, audioOnsetRate, audioTilt, audioZCR;\n"
       << "uniform float audioValence, audioArousal, audioTension, audioWarmth, audioSoftness, audioRoughness, audioCharm;\n"
       << "uniform vec2  audioMood;\n"
       << "uniform float audioEnergy, audioEnergyVel, audioEnergyAcc, audioBuildup, audioBuildupRate, audioDrop;\n"
       << "uniform float audioNovelty, audioSectionPhase, audioSectionAge, audioLayers, audioDensity;\n"
       << "uniform vec4  audioPresence;\n"
       << "uniform vec2  audioFlow;\n"
       << "uniform vec3  audioPalShadow, audioPalMid, audioPalHigh, audioPalAccent;\n"
       << "uniform float audioPalTemp, audioPalSat;\n"
       << "uniform float audioDominantPitch, audioMajorMinor, audioHCDF;\n"
       << "uniform float audioBassHit, audioMidHit, audioHighHit;\n"
       << "uniform float audioBassPresence, audioMidPresence, audioHighPresence, audioLevelPresence;\n"
       << "uniform float audioBassTime, audioMidTime, audioHighTime, audioTime;\n"
       << "uniform float audioBPMConfidence, audioPhase2, audioPhase4, audioPhase8, audioPhase16;\n"
       << "uniform float audioOnBeat, audioToggleOnBeat;\n"
       << "uniform float stemBass, stemDrums, stemMelody, stemAir, stemVocal;\n"
       << "uniform float stemBassHit, stemDrumsHit, stemMelodyHit, stemAirHit, stemVocalHit;\n"
       << "uniform float stemBassPresence, stemDrumsPresence, stemMelodyPresence, stemAirPresence, stemVocalPresence;\n";
    for (auto& t : targets_) fs << "uniform sampler2D " << t.name << ";\n";
    for (auto& p : params_) {
        if (p.type == ShaderParam::Image)
            fs << "uniform vec2 IMG_SIZE_" << p.name << ";\n"
               << "uniform bool _flip_" << p.name << ";\n";
        if (p.type == ShaderParam::Text) {
            fs << "uniform int " << p.name << "_len;\n";
            for (int i = 0; i < p.maxLen; i++)
                fs << "uniform int " << p.name << "_" << i << ";\n";
        }
    }
    for (auto& p : params_) {
        switch (p.type) {
            case ShaderParam::Float: fs << "uniform float " << p.name << ";\n"; break;
            case ShaderParam::Event:
            case ShaderParam::Bool: fs << "uniform bool " << p.name << ";\n"; break;
            case ShaderParam::Long: fs << "uniform int " << p.name << ";\n"; break;
            case ShaderParam::Color: fs << "uniform vec4 " << p.name << ";\n"; break;
            case ShaderParam::Point2D: fs << "uniform vec2 " << p.name << ";\n"; break;
            case ShaderParam::Image: fs << "uniform sampler2D " << p.name << ";\n"; break;
            case ShaderParam::Text: break; // declared above as int arrays
        }
    }
    fs << "#define IMG_NORM_PIXEL(img, coord) texture(img, coord)\n"
       << "#define IMG_PIXEL(img, coord) texture(img, (coord) / RENDERSIZE)\n"
       << "#define IMG_THIS_PIXEL(img) texture(img, gl_FragCoord.xy / RENDERSIZE)\n"
       << "#define IMG_THIS_NORM_PIXEL(img) texture(img, isf_FragNormCoord)\n"
       << "#define IMG_SIZE(img) RENDERSIZE\n"
       << "float audioSpectrum(float f){ return texture(audioFFT, vec2(clamp(f,0.0,1.0),0.5)).r; }\n"
       << "vec3 audioPalette(float t){\n"
          "  t = clamp(t,0.0,1.0);\n"
          "  vec3 c = (t<0.5)? mix(audioPalShadow,audioPalMid,t*2.0)\n"
          "                  : mix(audioPalMid,audioPalHigh,t*2.0-1.0);\n"
          "  return mix(c, audioPalAccent, audioBeat*smoothstep(0.6,1.0,t)*0.6);\n"
          "}\n"
       << "float audioKick(){ return audioBeatPulse; }\n"
       << "float audioHit(){ return audioOnset; }\n"
       << "float audioBreath(){ return pow(max(audioLevel,0.0),0.6); }\n"
       << "float audioAlive(float rest,float drive,float amount){ return rest+drive*amount; }\n";

    // body cleanup: strip #version / precision, texture2D → texture
    body = std::regex_replace(body, std::regex(R"(#version\s+\d+[^\n]*)"), "");
    body = std::regex_replace(body,
        std::regex(R"(precision\s+(highp|mediump|lowp)\s+\w+\s*;)"), "");
    body = std::regex_replace(body, std::regex(R"(\btexture2D\b)"), "texture");
    fs << body;

    if (!compile(fs.str())) return false;
    current_ = idx;
    frame_ = 0;
    lastTime_ = 0;
    allocTargets();
    return true;
}

bool ShaderHost::compile(const std::string& fragSrc) {
    auto sh = [&](GLenum type, const char* src) -> GLuint {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            lastError = log;
            glDeleteShader(s);
            return 0;
        }
        return s;
    };
    GLuint v = sh(GL_VERTEX_SHADER, kVertexSrc);
    if (!v) return false;
    GLuint f = sh(GL_FRAGMENT_SHADER, fragSrc.c_str());
    if (!f) { glDeleteShader(v); return false; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        lastError = log;
        glDeleteProgram(p);
        return false;
    }
    prog_ = p;
    return true;
}

static GLuint makeTex(int w, int h) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

void ShaderHost::allocTargets() {
    for (auto& t : targets_) {
        for (int i = 0; i < 2; i++) {
            t.tex[i] = makeTex(renderW, renderH);
            glGenFramebuffers(1, &t.fbo[i]);
            glBindFramebuffer(GL_FRAMEBUFFER, t.fbo[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, t.tex[i], 0);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        t.front = 0;
    }
    outTex_ = makeTex(renderW, renderH);
    glGenFramebuffers(1, &outFbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, outFbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTex_, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (!fftTex_) {
        glGenTextures(1, &fftTex_);
        glBindTexture(GL_TEXTURE_2D, fftTex_);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 256, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    }
    if (!blackTex_) {
        blackTex_ = makeTex(1, 1);
        unsigned char px[4] = {0, 0, 0, 255};
        glBindTexture(GL_TEXTURE_2D, blackTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    }
    if (!vao_) glGenVertexArrays(1, &vao_);
}

void ShaderHost::destroyGL() {
    for (auto& t : targets_) {
        glDeleteTextures(2, t.tex);
        glDeleteFramebuffers(2, t.fbo);
    }
    targets_.clear();
    if (outTex_) { glDeleteTextures(1, &outTex_); outTex_ = 0; }
    if (outFbo_) { glDeleteFramebuffers(1, &outFbo_); outFbo_ = 0; }
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
}

void ShaderHost::unload() {
    destroyGL();
    current_ = -1;
    params_.clear();
    passes_.clear();
}

void ShaderHost::render(float timeSec, const AudioUniforms& au,
                        float mouseX, float mouseY, float mouseDown) {
    if (!prog_) return;
    glUseProgram(prog_);
    auto U = [&](const char* n) { return glGetUniformLocation(prog_, n); };

    // fft upload
    if (au.spectrum) {
        unsigned char buf[256];
        for (int i = 0; i < 256; i++)
            buf[i] = (unsigned char)(std::min(1.0f, std::max(0.0f, au.spectrum[i])) * 255.0f);
        glActiveTexture(GL_TEXTURE0 + 15);
        glBindTexture(GL_TEXTURE_2D, fftTex_);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RED, GL_UNSIGNED_BYTE, buf);
        glUniform1i(U("audioFFT"), 15);
    }

    float dt = std::min(0.1f, std::max(0.0001f, timeSec - lastTime_));
    glUniform1f(U("TIME"), timeSec);
    glUniform1f(U("TIMEDELTA"), dt);
    lastTime_ = timeSec;
    glUniform2f(U("RENDERSIZE"), (float)renderW, (float)renderH);
    glUniform1i(U("FRAMEINDEX"), frame_++);
    glUniform2f(U("mousePos"), mouseX, mouseY);
    glUniform2f(U("mouseDelta"), 0, 0);
    glUniform1f(U("mouseDown"), mouseDown);
    glUniform1f(U("pinchHold"), 0);
    glUniform1f(U("audioLevel"), au.level);
    glUniform1f(U("audioBass"), au.bass);
    glUniform1f(U("audioMid"), au.mid);
    glUniform1f(U("audioHigh"), au.high);
    glUniform1f(U("useFontAtlas"), 0);
    glUniform1f(U("_voiceGlitch"), 0);
    glUniform1f(U("_transparentBg"), 0);

    // ── live audio feature bus (the whole point: shaders move with the music) ──
    bassClk_ += dt * (0.2 + au.bass * 2.0);
    midClk_ += dt * (0.2 + au.mid * 2.0);
    highClk_ += dt * (0.2 + au.high * 2.0);
    lvlClk_ += dt * (0.2 + au.level * 2.0);
    beatClk_ += dt * au.bpm / 60.0;
    auto frac = [](double v) { return (float)(v - std::floor(v)); };
    glUniform1f(U("audioSub"), au.sub);
    glUniform1f(U("audioLowMid"), au.lowMid);
    glUniform1f(U("audioHighMid"), au.mid);
    glUniform1f(U("audioTreble"), au.high);
    glUniform1f(U("audioPunch"), au.onset);
    glUniform1f(U("audioOnset"), au.onset);
    glUniform1f(U("audioFlux"), au.onset);
    glUniform1f(U("audioBeat"), au.onset);
    glUniform1f(U("audioBeatPulse"), au.onset);
    glUniform1f(U("audioBPM"), au.bpm);
    glUniform1f(U("audioTempo01"), std::min(1.0f, std::max(0.0f, (au.bpm - 60.0f) / 120.0f)));
    glUniform1f(U("audioBPMConfidence"), 0.8f);
    glUniform1f(U("audioBeatPhase"), frac(beatClk_));
    glUniform1f(U("audioBarPhase"), frac(beatClk_ / 4.0));
    glUniform1f(U("audioPhase2"), frac(beatClk_ / 2.0));
    glUniform1f(U("audioPhase4"), frac(beatClk_ / 4.0));
    glUniform1f(U("audioPhase8"), frac(beatClk_ / 8.0));
    glUniform1f(U("audioPhase16"), frac(beatClk_ / 16.0));
    glUniform1f(U("audioOnBeat"), frac(beatClk_) < 0.15f ? 1.0f : 0.0f);
    glUniform1f(U("audioToggleOnBeat"), ((long)beatClk_ % 2) ? 1.0f : 0.0f);
    glUniform1f(U("audioEnergy"), au.level);
    glUniform1f(U("audioArousal"), au.level);
    glUniform1f(U("audioValence"), 0.5f);
    glUniform2f(U("audioMood"), 0.5f, au.level);
    glUniform1f(U("audioBrightness"), au.high);
    glUniform1f(U("audioDensity"), au.level);
    glUniform1f(U("audioSectionPhase"), frac(timeSec / 32.0));
    glUniform1f(U("audioSectionAge"), std::fmod(timeSec, 32.0f));
    glUniform4f(U("audioPresence"), au.bass, au.mid, au.high, au.level);
    glUniform1f(U("audioBassPresence"), au.bass);
    glUniform1f(U("audioMidPresence"), au.mid);
    glUniform1f(U("audioHighPresence"), au.high);
    glUniform1f(U("audioLevelPresence"), au.level);
    glUniform1f(U("audioBassHit"), au.onset * au.bass);
    glUniform1f(U("audioMidHit"), au.onset * au.mid);
    glUniform1f(U("audioHighHit"), au.onset * au.high);
    glUniform1f(U("audioBassTime"), (float)bassClk_);
    glUniform1f(U("audioMidTime"), (float)midClk_);
    glUniform1f(U("audioHighTime"), (float)highClk_);
    glUniform1f(U("audioTime"), (float)lvlClk_);
    glUniform1f(U("stemBass"), au.bass);
    glUniform1f(U("stemDrums"), au.onset);
    glUniform1f(U("stemMelody"), au.mid);
    glUniform1f(U("stemAir"), au.high);
    glUniform1f(U("stemVocal"), au.mid);
    glUniform1f(U("stemBassHit"), au.onset * au.bass);
    glUniform1f(U("stemDrumsHit"), au.onset);
    glUniform1f(U("stemMelodyHit"), au.onset * au.mid);
    glUniform1f(U("stemAirHit"), au.onset * au.high);
    glUniform1f(U("stemVocalHit"), au.onset * au.mid);
    glUniform1f(U("stemBassPresence"), au.bass);
    glUniform1f(U("stemDrumsPresence"), au.onset);
    glUniform1f(U("stemMelodyPresence"), au.mid);
    glUniform1f(U("stemAirPresence"), au.high);
    glUniform1f(U("stemVocalPresence"), au.mid);
    // monochrome palette anchors — the OS is black & white by design
    glUniform3f(U("audioPalShadow"), 0.03f, 0.03f, 0.035f);
    glUniform3f(U("audioPalMid"), 0.42f, 0.42f, 0.44f);
    glUniform3f(U("audioPalHigh"), 0.92f, 0.92f, 0.94f);
    glUniform3f(U("audioPalAccent"), 1.0f, 1.0f, 1.0f);
    glUniform1f(U("audioPalTemp"), 0.5f);
    glUniform1f(U("audioPalSat"), 0.0f);

    // params
    for (auto& p : params_) {
        GLint loc = U(p.name.c_str());
        if (loc < 0) continue;
        switch (p.type) {
            case ShaderParam::Float:
            case ShaderParam::Event: glUniform1f(loc, p.cur[0]); break;
            case ShaderParam::Bool: glUniform1i(loc, p.cur[0] > 0.5f ? 1 : 0); break;
            case ShaderParam::Long: glUniform1i(loc, (int)p.cur[0]); break;
            case ShaderParam::Color: glUniform4fv(loc, 1, p.cur); break;
            case ShaderParam::Point2D: glUniform2fv(loc, 1, p.cur); break;
            case ShaderParam::Image: break; // bound below
            case ShaderParam::Text: break; // handled below (loc is _len-less name)
        }
    }
    for (auto& p : params_) {
        if (p.type == ShaderParam::Image) {
            glUniform2f(U(("IMG_SIZE_" + p.name).c_str()), (float)renderW, (float)renderH);
        } else if (p.type == ShaderParam::Text) {
            int len = std::min((int)p.text.size(), p.maxLen);
            glUniform1i(U((p.name + "_len").c_str()), len);
            for (int i = 0; i < p.maxLen; i++) {
                char nm[80];
                snprintf(nm, sizeof(nm), "%s_%d", p.name.c_str(), i);
                glUniform1i(U(nm), i < len ? (int)(unsigned char)p.text[i] : 0);
            }
        }
    }

    // static texture units: 0..7 targets, 8..12 aux black, 15 fft
    int unit = 8;
    const char* auxNames[] = {"varFontTex", "fontAtlasTex", "mpHandLandmarks",
                              "mpFaceLandmarks", "mpPoseLandmarks", "mpSegMask"};
    for (const char* n : auxNames) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, blackTex_);
        glUniform1i(U(n), unit);
        unit++;
    }
    for (auto& p : params_) {
        if (p.type != ShaderParam::Image) continue;
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, blackTex_);
        glUniform1i(U(p.name.c_str()), unit);
        unit++;
        if (unit > 14) break;
    }

    glBindVertexArray(vao_);
    glDisable(GL_BLEND);
    glViewport(0, 0, renderW, renderH);

    int lastTargetIdx = -1;
    for (int pi = 0; pi < (int)passes_.size(); pi++) {
        Pass& pass = passes_[pi];
        glUniform1i(U("PASSINDEX"), pi);

        // bind all target textures for reading (write-target reads its back buffer)
        for (int ti = 0; ti < (int)targets_.size(); ti++) {
            Target& t = targets_[ti];
            int readIdx = (ti == pass.targetIdx) ? 1 - t.front : t.front;
            // while writing t.front^1 below we read t.front; see swap logic
            readIdx = t.front;
            glActiveTexture(GL_TEXTURE0 + ti);
            glBindTexture(GL_TEXTURE_2D, t.tex[readIdx]);
            glUniform1i(U(t.name.c_str()), ti);
        }

        if (pass.targetIdx >= 0) {
            Target& t = targets_[pass.targetIdx];
            int write = 1 - t.front;
            glBindFramebuffer(GL_FRAMEBUFFER, t.fbo[write]);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            t.front = write;
            lastTargetIdx = pass.targetIdx;
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, outFbo_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            lastTargetIdx = -1;
        }
    }

    // if the final pass wrote to a named target, copy it to the output
    if (lastTargetIdx >= 0) {
        Target& t = targets_[lastTargetIdx];
        glBindFramebuffer(GL_READ_FRAMEBUFFER, t.fbo[t.front]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outFbo_);
        glBlitFramebuffer(0, 0, renderW, renderH, 0, 0, renderW, renderH,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
}

// ── fullscreen textured blit (per-context resources) ──
struct BlitCtx { GLuint prog = 0, vao = 0; };
static std::map<void*, BlitCtx> gBlitCtxs;
extern "C" void* glfwGetCurrentContext(void);

void ShaderHost::drawFullscreen(GLuint tex, float dim, float vignette) {
    void* ctx = glfwGetCurrentContext();
    BlitCtx& bc = gBlitCtxs[ctx];
    if (!bc.prog) {
        static const char* vs = R"(#version 330 core
const vec2 v[4] = vec2[4](vec2(-1,-1), vec2(1,-1), vec2(-1,1), vec2(1,1));
out vec2 uv;
void main(){ uv = v[gl_VertexID]*0.5+0.5; gl_Position = vec4(v[gl_VertexID],0,1); }
)";
        static const char* fsrc = R"(#version 330 core
in vec2 uv; out vec4 o;
uniform sampler2D t; uniform float dim; uniform float vig;
void main(){
    vec3 c = texture(t, uv).rgb * dim;
    // black edge fade: vig 0 = off, 1 = fade reaching deep into the frame
    if (vig > 0.001) {
        float d = distance(uv, vec2(0.5)) * 1.4142; // 0 center → 1 corner
        c *= 1.0 - smoothstep(mix(1.0, 0.12, vig), 1.02, d);
    }
    o = vec4(c, 1.0);
}
)";
        GLuint v = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(v, 1, &vs, nullptr); glCompileShader(v);
        GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(f, 1, &fsrc, nullptr); glCompileShader(f);
        bc.prog = glCreateProgram();
        glAttachShader(bc.prog, v); glAttachShader(bc.prog, f);
        glLinkProgram(bc.prog);
        glDeleteShader(v); glDeleteShader(f);
        glGenVertexArrays(1, &bc.vao);
    }
    glUseProgram(bc.prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(glGetUniformLocation(bc.prog, "t"), 0);
    glUniform1f(glGetUniformLocation(bc.prog, "dim"), dim);
    glUniform1f(glGetUniformLocation(bc.prog, "vig"), vignette);
    glBindVertexArray(bc.vao);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glEnable(GL_BLEND);
}
