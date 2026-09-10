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

    int hopsSinceTempo_ = 0, hopsSinceKey_ = 0;
    int samplePos_ = 0;
};
