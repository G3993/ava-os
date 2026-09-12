#include "analysis.h"
#include <cmath>
#include <cstring>
#include <algorithm>

static const float kMajorProfile[12] = {6.35f,2.23f,3.48f,2.33f,4.38f,4.09f,2.52f,5.19f,2.39f,3.66f,2.29f,2.88f};
static const float kMinorProfile[12] = {6.33f,2.68f,3.52f,5.38f,2.60f,3.53f,2.54f,4.75f,3.98f,2.69f,3.34f,3.17f};

static float noteToHzOct1(int semitone) {
    // octave 1: midi = 24 + semitone (C1=24); A440 tuning
    int midi = 24 + semitone;
    return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f);
}

static float transposeToMidOctave(float hz) {
    while (hz < 45.0f) hz *= 2.0f;
    while (hz > 70.0f) hz /= 2.0f;
    return hz;
}

static float pearson(const float* a, const float* b, int n) {
    float ma = 0, mb = 0;
    for (int i = 0; i < n; i++) { ma += a[i]; mb += b[i]; }
    ma /= n; mb /= n;
    float num = 0, da = 0, db = 0;
    for (int i = 0; i < n; i++) {
        float xa = a[i] - ma, xb = b[i] - mb;
        num += xa * xb; da += xa * xa; db += xb * xb;
    }
    float den = std::sqrt(da * db);
    return den > 1e-9f ? num / den : 0.0f;
}

void StreamAnalyzer::init(float sampleRate) {
    sr_ = sampleRate;
    monoRing_.assign(kFFT, 0.0f);
    monoW_ = 0; hopCount_ = 0;

    window_.resize(kFFT);
#ifdef __APPLE__
    fftSetup_ = vDSP_create_fftsetup(11, kFFTRadix2); // 2^11 = 2048
    vDSP_hann_window(window_.data(), kFFT, vDSP_HANN_NORM);
#else
    for (int i = 0; i < kFFT; i++)
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * i / kFFT));
#endif
    fftReal_.assign(kFFT / 2, 0.0f);
    fftImag_.assign(kFFT / 2, 0.0f);
    mag_.assign(kFFT / 2, 0.0f);
    prevMag_.assign(kFFT / 2, 0.0f);

    // ~8 s of flux history at hop rate
    int histLen = (int)(8.0f * sr_ / kHop);
    fluxHist_.assign(histLen, 0.0f);
    fluxW_ = 0; fluxCount_ = 0;
}

StreamAnalyzer::~StreamAnalyzer() {
#ifdef __APPLE__
    if (fftSetup_) vDSP_destroy_fftsetup(fftSetup_);
#endif
}

float StreamAnalyzer::pushSample(float x) {
    monoRing_[monoW_] = x;
    monoW_ = (monoW_ + 1) % kFFT;
    samplesSinceOnset_++;
    samplePos_++;

    float onset = 0.0f;
    if (++hopCount_ >= kHop) {
        hopCount_ = 0;
        processHop();
        if (pendingOnset_ > 0.0f) {
            onset = pendingOnset_;
            pendingOnset_ = 0.0f;
        }
    }
    return onset;
}

void StreamAnalyzer::processHop() {
    // assemble windowed frame ending at monoW_
    float frame[kFFT];
    int start = monoW_; // oldest sample
    for (int i = 0; i < kFFT; i++)
        frame[i] = monoRing_[(start + i) % kFFT] * window_[i];

    // real FFT
#ifdef __APPLE__
    DSPSplitComplex split{fftReal_.data(), fftImag_.data()};
    vDSP_ctoz((const DSPComplex*)frame, 2, &split, 1, kFFT / 2);
    vDSP_fft_zrip(fftSetup_, &split, 1, 11, kFFTDirection_Forward);
    // magnitudes
    split.imagp[0] = 0.0f; // pack: imag[0] holds Nyquist; drop for magnitude
    vDSP_zvabs(&split, 1, mag_.data(), 1, kFFT / 2);
#else
    fft_.magnitudes(frame, mag_.data());
#endif

    // ── spectral flux (positive differences), bass-weighted ──
    // Full-band flux fires on hi-hats and vocal consonants, which makes the
    // pulses feel detached from the groove. Weighting toward the low end
    // locks onsets — and the tempo track built on them — to kick and bass.
    float flux = 0.0f;
    float fluxBinHz = sr_ / kFFT;
    for (int i = 1; i < kFFT / 2; i++) {
        float d = mag_[i] - prevMag_[i];
        if (d > 0) {
            float f = i * fluxBinHz;
            flux += d * (f < 300.0f ? 1.0f : 300.0f / f);
        }
    }
    // ── SPLIT: per-band strongest partial + per-band flux ──
    {
        static const float lo[3] = {40.0f, 160.0f, 900.0f};
        static const float hi[3] = {160.0f, 900.0f, 6000.0f};
        for (int b = 0; b < 3; b++) {
            int i0 = std::max(1, (int)(lo[b] / fluxBinHz)), i1 = std::min(kFFT / 2 - 2, (int)(hi[b] / fluxBinHz));
            int best = i0; float bm = 0, bflux = 0;
            for (int i = i0; i <= i1; i++) {
                // tilt so the higher bands don't always pick their lowest bin
                float wgt = mag_[i] * std::sqrt((float)i / i0);
                if (wgt > bm) { bm = wgt; best = i; }
                float d = mag_[i] - prevMag_[i];
                if (d > 0) bflux += d;
            }
            float m = mag_[best];
            // parabolic interpolation around the peak bin
            float a = mag_[best - 1], c = mag_[best + 1];
            float den = a - 2 * m + c;
            float off = std::fabs(den) > 1e-9f ? 0.5f * (a - c) / den : 0.0f;
            float hz = (best + std::max(-0.5f, std::min(0.5f, off))) * fluxBinHz;
            bandMagMax_[b] = std::max(m, bandMagMax_[b] * 0.9993f); // ~15 s at hop rate
            float rel = m / bandMagMax_[b];
            // only accept a new pitch when the partial is real, else hold the last
            if (rel > 0.12f) {
                float target = hz;
                // reject single-hop octave-ish jumps unless they persist (light smoothing)
                bandHzSm_[b] += (target - bandHzSm_[b]) * 0.35f;
            }
            out.bandPeakHz[b].store(bandHzSm_[b]);
            out.bandPeakMag[b].store(std::min(1.0f, rel));
            // second partial: strongest bin at least ±12% (a whole tone) from the first
            {
                int best2 = -1; float bm2 = 0;
                for (int i = i0; i <= i1; i++) {
                    if (std::fabs((float)i - best) < best * 0.12f) continue;
                    float wgt = mag_[i] * std::sqrt((float)i / i0);
                    if (wgt > bm2) { bm2 = wgt; best2 = i; }
                }
                if (best2 > 0) {
                    float m2 = mag_[best2];
                    float a2 = mag_[best2 - 1], c2 = mag_[best2 + 1];
                    float den2 = a2 - 2 * m2 + c2;
                    float off2 = std::fabs(den2) > 1e-9f ? 0.5f * (a2 - c2) / den2 : 0.0f;
                    float hz2 = (best2 + std::max(-0.5f, std::min(0.5f, off2))) * fluxBinHz;
                    float rel2 = m2 / bandMagMax_[b];
                    if (rel2 > 0.10f) bandHz2Sm_[b] += (hz2 - bandHz2Sm_[b]) * 0.3f;
                    out.bandPeak2Hz[b].store(bandHz2Sm_[b]);
                    out.bandPeak2Mag[b].store(std::min(1.0f, rel2));
                }
            }
            bandFluxMax_[b] = std::max(bflux, bandFluxMax_[b] * 0.9995f);
            out.bandOnset[b].store(bflux / bandFluxMax_[b]);
        }
    }
    std::memcpy(prevMag_.data(), mag_.data(), sizeof(float) * kFFT / 2);

    fluxHist_[fluxW_] = flux;
    fluxW_ = (fluxW_ + 1) % (int)fluxHist_.size();
    if (fluxCount_ < (int)fluxHist_.size()) fluxCount_++;
    fluxRunMax_ = std::max(flux, fluxRunMax_ * 0.9995f);

    // onset: flux above mean + 1.5*std of the last ~1 s, refractory 90 ms
    int lookback = std::min(fluxCount_, (int)(1.0f * sr_ / kHop));
    if (lookback > 8) {
        float mean = 0;
        for (int i = 1; i <= lookback; i++)
            mean += fluxHist_[(fluxW_ - 1 - i + (int)fluxHist_.size()) % (int)fluxHist_.size()];
        mean /= lookback;
        float var = 0;
        for (int i = 1; i <= lookback; i++) {
            float v = fluxHist_[(fluxW_ - 1 - i + (int)fluxHist_.size()) % (int)fluxHist_.size()] - mean;
            var += v * v;
        }
        float std = std::sqrt(var / lookback);
        bool refractoryOver = samplesSinceOnset_ > (int)(0.09f * sr_);
        if (refractoryOver && flux > mean + 1.5f * std && flux > 0.02f * fluxRunMax_) {
            pendingOnset_ = std::min(1.0f, flux / fluxRunMax_);
            samplesSinceOnset_ = 0;
            out.onsetFlash.store(1.0f);
        }
    }
    float flash = out.onsetFlash.load();
    if (flash > 0) out.onsetFlash.store(flash * 0.92f);

    // ── spectrum snapshot for shaders (256 bins, AGC-normalized, soft curve) ──
    {
        static thread_local float specMax = 1e-5f;
        float frameMax = 0;
        const int group = (kFFT / 2) / kSpecBins; // 4 bins per output
        for (int o = 0; o < kSpecBins; o++) {
            float acc = 0;
            for (int j = 0; j < group; j++) acc += mag_[o * group + j];
            acc /= group;
            frameMax = std::max(frameMax, acc);
            uiSpectrum[o] = acc; // normalized below
        }
        specMax = std::max(frameMax, specMax * 0.999f);
        float inv = specMax > 1e-6f ? 1.0f / specMax : 0.0f;
        for (int o = 0; o < kSpecBins; o++)
            uiSpectrum[o] = std::sqrt(std::min(1.0f, uiSpectrum[o] * inv));
    }

    // ── chroma accumulation (55–2000 Hz), slow decay ──
    float binHz = sr_ / kFFT;
    for (int i = 1; i < kFFT / 2; i++) {
        float f = i * binHz;
        if (f < 55.0f || f > 2000.0f) continue;
        int pc = (int)std::lround(12.0f * std::log2(f / 16.3515978313f)) % 12; // C0 ref
        chroma_[pc] += mag_[i];
    }
    for (int i = 0; i < 12; i++) chroma_[i] *= 0.995f;

    if (++hopsSinceTempo_ >= (int)(2.0f * sr_ / kHop)) { hopsSinceTempo_ = 0; updateTempo(); }
    if (++hopsSinceKey_ >= (int)(0.5f * sr_ / kHop)) { hopsSinceKey_ = 0; updateKey(); }
}

void StreamAnalyzer::updateTempo() {
    if (fluxCount_ < (int)fluxHist_.size() / 2) return;
    float hopRate = sr_ / kHop;                       // ~93.75 Hz
    int n = fluxCount_;
    // linear copy oldest→newest
    std::vector<float> h(n);
    for (int i = 0; i < n; i++)
        h[i] = fluxHist_[(fluxW_ + i + (int)fluxHist_.size() - n) % (int)fluxHist_.size()];
    // remove mean
    float mean = 0; for (float v : h) mean += v; mean /= n;
    for (float& v : h) v -= mean;

    int lagMin = (int)(hopRate * 60.0f / 180.0f);     // 180 BPM
    int lagMax = (int)(hopRate * 60.0f / 60.0f);      // 60 BPM
    float best = -1e30f; int bestLag = (int)(hopRate * 0.5f);
    for (int lag = lagMin; lag <= lagMax && lag < n / 2; lag++) {
        float acc = 0;
        for (int i = 0; i + lag < n; i++) acc += h[i] * h[i + lag];
        float bpm = 60.0f * hopRate / lag;
        // log-gaussian prior around 120 BPM (librosa-style)
        float prior = std::exp(-0.5f * std::pow(std::log2(bpm / 120.0f) / 1.0f, 2.0f));
        acc *= prior;
        if (acc > best) { best = acc; bestLag = lag; }
    }
    if (best > 0) {
        float bpm = 60.0f * hopRate / bestLag;
        float cur = out.bpm.load();
        out.bpm.store(cur * 0.6f + bpm * 0.4f);
    }
}

void StreamAnalyzer::updateKey() {
    float total = 0; for (int i = 0; i < 12; i++) total += chroma_[i];
    if (total < 1e-5f) return;

    float bestScore = -1e30f; int bestKey = 9, bestMinor = 0;
    float rotated[12];
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 12; j++) rotated[j] = chroma_[(j + i) % 12];
        float maj = pearson(rotated, kMajorProfile, 12);
        float min = pearson(rotated, kMinorProfile, 12);
        if (maj > bestScore) { bestScore = maj; bestKey = i; bestMinor = 0; }
        if (min > bestScore) { bestScore = min; bestKey = i; bestMinor = 1; }
    }
    out.keyIndex.store(bestKey);
    out.keyMinor.store(bestMinor);
    out.midOctaveHz.store(transposeToMidOctave(noteToHzOct1(bestKey)));
}
