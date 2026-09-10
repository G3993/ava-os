// Portable 2048-point radix-2 FFT used on non-Apple builds in place of vDSP.
// Absolute scale differs from vDSP_fft_zrip; downstream analysis only uses
// normalized magnitudes (flux / running max, chroma correlation) so it cancels.
#pragma once
#include <cmath>
#include <vector>

namespace rfft {

struct FFT2048 {
    static constexpr int N = 2048;
    static constexpr int kBits = 11;

    FFT2048() : re_(N), im_(N), cosT_(N / 2), sinT_(N / 2), rev_(N) {
        const double pi = 3.14159265358979323846;
        for (int i = 0; i < N / 2; i++) {
            cosT_[i] = (float)std::cos(-2.0 * pi * i / N);
            sinT_[i] = (float)std::sin(-2.0 * pi * i / N);
        }
        for (int i = 0; i < N; i++) {
            int r = 0;
            for (int b = 0; b < kBits; b++)
                if (i & (1 << b)) r |= 1 << (kBits - 1 - b);
            rev_[i] = r;
        }
    }

    // in: N real samples → mag: N/2 magnitudes (bins 0..N/2-1)
    void magnitudes(const float* in, float* mag) {
        for (int i = 0; i < N; i++) { re_[rev_[i]] = in[i]; im_[rev_[i]] = 0.0f; }
        for (int len = 2; len <= N; len <<= 1) {
            int half = len >> 1, step = N / len;
            for (int i = 0; i < N; i += len)
                for (int j = 0; j < half; j++) {
                    float wr = cosT_[j * step], wi = sinT_[j * step];
                    float xr = re_[i + j + half] * wr - im_[i + j + half] * wi;
                    float xi = re_[i + j + half] * wi + im_[i + j + half] * wr;
                    re_[i + j + half] = re_[i + j] - xr;
                    im_[i + j + half] = im_[i + j] - xi;
                    re_[i + j] += xr;
                    im_[i + j] += xi;
                }
        }
        for (int k = 0; k < N / 2; k++)
            mag[k] = std::sqrt(re_[k] * re_[k] + im_[k] * im_[k]);
    }

private:
    std::vector<float> re_, im_, cosT_, sinT_;
    std::vector<int> rev_;
};

} // namespace rfft
