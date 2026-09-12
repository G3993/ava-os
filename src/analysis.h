// AVA OS — streaming musical analysis
// Real-time port of the offline analyzer: onset detection (spectral flux),
// tempo (autocorrelation of flux history), key/root (chroma + Krumhansl-
// Schmuckler profiles), transposed to the 45-70 Hz "mid octave".
#pragma once
#include <atomic>
#include <vector>
#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#else
#include "rfft.h"
#endif

struct AnalyzerOut {
    std::atomic<float> bpm{120.0f};
    std::atomic<float> midOctaveHz{55.0f};
    std::atomic<int>   keyIndex{9};        // 0=C .. 11=B (9=A)
    std::atomic<int>   keyMinor{0};
    std::atomic<float> onsetFlash{0.0f};   // decaying UI indicator
    // SPLIT mode: strongest partial per musical layer, each hop (~10 ms).
    // Bands: 0 bass 40-160 · 1 vocal/chord 160-900 · 2 melody/air 900-6000.
    // peakHz = parabolic-interpolated bin centre; peakMag = relative strength
    // 0..1 vs that band's slow running max (so quiet layers still track).
    std::atomic<float> bandPeakHz[3]{{55.0f}, {220.0f}, {1760.0f}};
    std::atomic<float> bandPeakMag[3]{{0.0f}, {0.0f}, {0.0f}};
    std::atomic<float> bandOnset[3]{{0.0f}, {0.0f}, {0.0f}}; // per-band flux, normalized
    // second-strongest partial per band (at least a whole tone away from the
    // first): the harmony under the melody, for the HEART layer
    std::atomic<float> bandPeak2Hz[3]{{82.0f}, {330.0f}, {2640.0f}};
    std::atomic<float> bandPeak2Mag[3]{{0.0f}, {0.0f}, {0.0f}};
};

class StreamAnalyzer {
public:
    static constexpr int kFFT = 2048;
    static constexpr int kHop = 512;

    void init(float sampleRate);
    ~StreamAnalyzer();

    // Feed one mono sample. Returns onset strength >0 when an onset fires this sample.
    float pushSample(float x);

    AnalyzerOut out;

    // spectrum snapshot for shader audioFFT texture (written on the audio
    // thread each hop, read by the UI thread; tearing is harmless here)
    static constexpr int kSpecBins = 256;
    float uiSpectrum[kSpecBins] = {0};

private:
    void processHop();
    void updateTempo();
    void updateKey();

    float sr_ = 48000.0f;
    // hop accumulation + analysis ring of mono samples
    std::vector<float> monoRing_;
    int monoW_ = 0, hopCount_ = 0;

    // FFT (vDSP on macOS, portable radix-2 elsewhere)
#ifdef __APPLE__
    FFTSetup fftSetup_ = nullptr;
#else
    rfft::FFT2048 fft_;
#endif
    std::vector<float> window_, fftReal_, fftImag_, mag_, prevMag_;

    // onset / flux
    std::vector<float> fluxHist_;   // at hop rate
    int fluxW_ = 0, fluxCount_ = 0;
    float fluxRunMax_ = 1e-6f;
    int samplesSinceOnset_ = 1 << 30;
    float pendingOnset_ = 0.0f;

    // chroma
    float chroma_[12] = {0};
    // per-band peak tracking
    float bandMagMax_[3] = {1e-4f, 1e-4f, 1e-4f};
    float bandFluxMax_[3] = {1e-4f, 1e-4f, 1e-4f};
    float bandHzSm_[3] = {55.0f, 220.0f, 1760.0f};
    float bandHz2Sm_[3] = {82.0f, 330.0f, 2640.0f};

    int hopsSinceTempo_ = 0, hopsSinceKey_ = 0;
    int samplePos_ = 0;
};
