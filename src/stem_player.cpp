#include "stem_player.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <chrono>
#include <dirent.h>
#include <sys/stat.h>

static std::string lower(std::string s) { for (char& c : s) c = (char)tolower((unsigned char)c); return s; }
static bool has(const std::string& s, const char* k) { return s.find(k) != std::string::npos; }

// which stem a file name belongs to, or -1. Prefers the ADAM "_chN_" convention,
// then plain zone words. "head" must not match "headphones".
static int classify(const std::string& fileName, int* priority) {
    std::string n = lower(fileName);
    *priority = 1;
    if (has(n, "_ch1+2") || has(n, "ch1+2")) { *priority = 0; return 0; }
    if (has(n, "_ch3_")) { *priority = 0; return 1; }
    if (has(n, "_ch4_")) { *priority = 0; return 2; }
    if (has(n, "_ch5_")) { *priority = 0; return 3; }
    if (has(n, "_ch6_")) { *priority = 0; return 4; }
    if (has(n, "_ch7_")) { *priority = 0; return 5; }
    if (has(n, "headphone") || has(n, "_hp") || has(n, " hp") || has(n, "master") || has(n, "main") || has(n, "music")) return 0;
    if (has(n, "heart")) return 2;
    if (has(n, "head")) return 1;
    if (has(n, "belly")) return 3;
    if (has(n, "root") || has(n, "butt")) return 4;
    if (has(n, "feet") || has(n, "foot")) return 5;
    return -1;
}

StemPlayer::~StemPlayer() { unload(); }

bool StemPlayer::openWav(const std::string& path, Stem& s) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { lastError = "cannot open " + path; return false; }
    unsigned char hdr[12];
    if (fread(hdr, 1, 12, f) != 12 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
        fclose(f); lastError = "not a WAV file: " + path; return false;
    }
    bool gotFmt = false;
    while (true) {
        unsigned char ck[8];
        if (fread(ck, 1, 8, f) != 8) break;
        uint32_t len = ck[4] | (ck[5] << 8) | (ck[6] << 16) | ((uint32_t)ck[7] << 24);
        if (memcmp(ck, "fmt ", 4) == 0) {
            unsigned char fm[40] = {0};
            size_t take = std::min<size_t>(len, sizeof(fm));
            fread(fm, 1, take, f);
            if (len > take) fseek(f, (long)(len - take), SEEK_CUR);
            s.fmt = fm[0] | (fm[1] << 8);
            s.ch = fm[2] | (fm[3] << 8);
            s.sr = fm[4] | (fm[5] << 8) | (fm[6] << 16) | (fm[7] << 24);
            s.blockAlign = fm[12] | (fm[13] << 8);
            s.bits = fm[14] | (fm[15] << 8);
            if (s.fmt == 0xFFFE && len >= 26) s.fmt = fm[24] | (fm[25] << 8); // extensible: sub-format
            gotFmt = true;
        } else if (memcmp(ck, "data", 4) == 0) {
            s.dataOff = ftell(f);
            if (!gotFmt || s.blockAlign <= 0) { fclose(f); lastError = "bad WAV header: " + path; return false; }
            s.frames = (long)(len / (uint32_t)s.blockAlign);
            break;
        } else {
            fseek(f, (long)(len + (len & 1)), SEEK_CUR);
        }
        if (len & 1) fseek(f, 1, SEEK_CUR);
    }
    if (s.dataOff == 0) { fclose(f); lastError = "no data chunk: " + path; return false; }
    if (!((s.fmt == 1 && (s.bits == 16 || s.bits == 24 || s.bits == 32)) || (s.fmt == 3 && s.bits == 32))) {
        fclose(f); lastError = "unsupported WAV format (need 16/24/32-bit PCM or 32-bit float): " + path; return false;
    }
    if (s.sr != kSR) { fclose(f); lastError = "export at 48 kHz (this file is " + std::to_string(s.sr) + " Hz): " + path; return false; }
    s.f = f;
    s.readPos = 0;
    return true;
}

bool StemPlayer::load(const std::string& folderOrFile) {
    unload();
    std::string folder = folderOrFile;
    struct stat st;
    if (stat(folder.c_str(), &st) != 0) { lastError = "not found: " + folder; return false; }
    if (!S_ISDIR(st.st_mode)) {
        size_t sl = folder.find_last_of('/');
        folder = sl == std::string::npos ? "." : folder.substr(0, sl);
    }
    DIR* d = opendir(folder.c_str());
    if (!d) { lastError = "cannot read folder: " + folder; return false; }
    int bestPri[kStems]; for (int& p : bestPri) p = 99;
    std::string chosen[kStems];
    while (dirent* e = readdir(d)) {
        std::string fn = e->d_name;
        if (fn.empty() || fn[0] == '.') continue;
        std::string ln = lower(fn);
        if (ln.size() < 5 || (ln.substr(ln.size() - 4) != ".wav" && ln.substr(ln.size() - 5) != ".wave")) continue;
        int pri = 1;
        int k = classify(fn, &pri);
        if (k < 0) continue;
        if (pri < bestPri[k] || (pri == bestPri[k] && fn < chosen[k])) { bestPri[k] = pri; chosen[k] = fn; }
    }
    closedir(d);
    int found = 0;
    long maxFrames = 0;
    for (int k = 0; k < kStems; k++) {
        if (chosen[k].empty()) continue;
        std::string path = folder + "/" + chosen[k];
        if (!openWav(path, stems_[k])) { unload(); return false; }
        has[k] = true; stemFile[k] = chosen[k]; found++;
        maxFrames = std::max(maxFrames, stems_[k].frames);
    }
    if (found == 0) {
        lastError = "no stems in " + folder + " (expected WAVs named …_ch1+2_hp / _ch3_HEAD … or head / heart / belly / root / feet / master)";
        return false;
    }
    frames_ = maxFrames;
    size_t sl = folder.find_last_of('/');
    name = sl == std::string::npos ? folder : folder.substr(sl + 1);
    for (int k = 0; k < kStems; k++) { rings_[k].init(kSR * 2); stemPeak[k].store(0); }
    raw_.resize(4096 * 8 * 2);
    dec_.resize(4096 * 2);

    // leading silence: start where the first stem gets above -60 dBFS
    leadIn_ = frames_;
    for (int k = 0; k < kStems; k++) {
        if (!has[k]) continue;
        Stem& s = stems_[k];
        long pos = 0;
        std::vector<float> lr;
        fseek(s.f, s.dataOff, SEEK_SET);
        bool found1 = false;
        while (pos < s.frames && pos < leadIn_) {
            int n = (int)std::min<long>(4096, s.frames - pos);
            size_t got = fread(raw_.data(), (size_t)s.blockAlign, (size_t)n, s.f);
            if (got == 0) break;
            decodeInto(s, raw_.data(), (int)got, lr);
            for (int i = 0; i < (int)got; i++) {
                if (std::fabs(lr[i * 2]) > 0.001f || std::fabs(lr[i * 2 + 1]) > 0.001f) {
                    leadIn_ = std::min(leadIn_, pos + i); found1 = true; break;
                }
            }
            if (found1) break;
            pos += (long)got;
        }
    }
    if (leadIn_ >= frames_) leadIn_ = 0;
    playHead_.store(leadIn_);
    seekTo_.store(leadIn_);
    quit_.store(false);
    reader_ = std::thread([this] { readerLoop(); });
    return true;
}

void StemPlayer::unload() {
    state_.store(0);
    if (reader_.joinable()) { quit_.store(true); reader_.join(); }
    for (int k = 0; k < kStems; k++) {
        if (stems_[k].f) fclose(stems_[k].f);
        stems_[k] = Stem();
        has[k] = false; stemFile[k].clear();
    }
    frames_ = 0; leadIn_ = 0; playHead_.store(0); name.clear();
}

void StemPlayer::play() { if (loaded()) { if (state_.load() == 0 && playHead_.load() >= frames_) seek(leadInSec()); state_.store(1); } }
void StemPlayer::pause() { if (state_.load() == 1) state_.store(2); }
void StemPlayer::stop() { state_.store(0); seek(leadInSec()); }
void StemPlayer::seek(double sec) {
    if (!loaded()) return;
    long f = (long)std::max(0.0, std::min(sec, lengthSec())) * 1 ;
    f = (long)(std::max(0.0, std::min(sec, lengthSec())) * kSR);
    seekTo_.store(f);
}

void StemPlayer::decodeInto(Stem& s, const unsigned char* raw, int frames, std::vector<float>& lr) {
    lr.resize((size_t)frames * 2);
    const int bps = s.bits / 8;
    for (int i = 0; i < frames; i++) {
        const unsigned char* p = raw + (size_t)i * s.blockAlign;
        float v[2] = {0, 0};
        for (int c = 0; c < std::min(s.ch, 2); c++) {
            const unsigned char* q = p + c * bps;
            float x = 0;
            if (s.fmt == 3) { float fx; memcpy(&fx, q, 4); x = fx; }
            else if (s.bits == 16) { int16_t t = (int16_t)(q[0] | (q[1] << 8)); x = t / 32768.0f; }
            else if (s.bits == 24) { int32_t t = (q[0] << 8) | (q[1] << 16) | (q[2] << 24); x = (t >> 8) / 8388608.0f; }
            else { int32_t t; memcpy(&t, q, 4); x = t / 2147483648.0f; }
            v[c] = x;
        }
        if (s.ch == 1) v[1] = v[0];
        lr[(size_t)i * 2] = v[0]; lr[(size_t)i * 2 + 1] = v[1];
    }
}

void StemPlayer::fillStem(int k, int wantFrames) {
    Stem& s = stems_[k];
    while (rings_[k].available() < wantFrames && s.readPos < s.frames) {
        int n = (int)std::min<long>(4096, s.frames - s.readPos);
        fseek(s.f, s.dataOff + s.readPos * (long)s.blockAlign, SEEK_SET);
        size_t got = fread(raw_.data(), (size_t)s.blockAlign, (size_t)n, s.f);
        if (got == 0) { s.readPos = s.frames; break; }
        decodeInto(s, raw_.data(), (int)got, dec_);
        rings_[k].push(dec_.data(), (int)got);
        s.readPos += (long)got;
    }
}

void StemPlayer::readerLoop() {
    const int ahead = kSR;   // keep 1 s decoded per stem
    while (!quit_.load()) {
        long sk = seekTo_.exchange(-1);
        if (sk >= 0) {
            filling.store(true);
            for (int k = 0; k < kStems; k++) {
                if (!has[k]) continue;
                rings_[k].init(kSR * 2);
                stems_[k].readPos = std::min(sk, stems_[k].frames);
                fillStem(k, ahead / 2);
            }
            playHead_.store(sk);
            filling.store(false);
        }
        if (state_.load() != 0) for (int k = 0; k < kStems; k++) if (has[k]) fillStem(k, ahead);
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
}

bool StemPlayer::render(float* hpL, float* hpR, float* const* zones, int n) {
    int st = state_.load();
    if (!loaded() || st == 0) return false;
    auto silence = [&]() {
        for (int i = 0; i < n; i++) { hpL[i] = 0; hpR[i] = 0; }
        for (int z = 0; z < 5; z++) for (int i = 0; i < n; i++) zones[z][i] = 0;
    };
    if (st == 2 || filling.load()) { silence(); return true; }
    long head = playHead_.load();
    if (head >= frames_) { silence(); state_.store(0); return true; }   // end of set
    static thread_local std::vector<float> tmpL, tmpR;
    tmpL.resize(n); tmpR.resize(n);
    for (int k = 0; k < kStems; k++) {
        float* dl = k == 0 ? hpL : zones[k - 1];
        float* dr = k == 0 ? hpR : tmpR.data();
        if (!has[k]) { for (int i = 0; i < n; i++) dl[i] = 0; if (k == 0) for (int i = 0; i < n; i++) dr[i] = 0; continue; }
        rings_[k].pop(dl, dr, n);           // zero-fills a shortfall (disk hiccup)
        if (k > 0) for (int i = 0; i < n; i++) dl[i] = 0.5f * (dl[i] + dr[i]);   // zone stems are mono content
        float pk = 0;
        for (int i = 0; i < n; i++) pk = std::max(pk, std::fabs(dl[i]));
        float prev = stemPeak[k].load(std::memory_order_relaxed) * 0.9f;
        stemPeak[k].store(pk > prev ? pk : prev, std::memory_order_relaxed);
    }
    playHead_.store(head + n);
    return true;
}
