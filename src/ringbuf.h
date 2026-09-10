// Lock-free SPSC ring buffer for interleaved stereo float frames
#pragma once
#include <atomic>
#include <cstring>
#include <vector>

class StereoRing {
public:
    void init(int frames) {
        n_ = frames;
        buf_.assign((size_t)frames * 2, 0.0f);
        r_.store(0); w_.store(0);
    }
    // producer: push interleaved stereo frames (drops on overflow)
    void push(const float* lr, int frames) {
        int w = w_.load(std::memory_order_relaxed);
        int r = r_.load(std::memory_order_acquire);
        int free = n_ - 1 - ((w - r + n_) % n_);
        if (frames > free) frames = free;
        for (int i = 0; i < frames; i++) {
            buf_[(size_t)w * 2]     = lr[i * 2];
            buf_[(size_t)w * 2 + 1] = lr[i * 2 + 1];
            w = (w + 1) % n_;
        }
        w_.store(w, std::memory_order_release);
    }
    // producer: push planar
    void pushPlanar(const float* L, const float* R, int frames) {
        int w = w_.load(std::memory_order_relaxed);
        int r = r_.load(std::memory_order_acquire);
        int free = n_ - 1 - ((w - r + n_) % n_);
        if (frames > free) frames = free;
        for (int i = 0; i < frames; i++) {
            buf_[(size_t)w * 2]     = L[i];
            buf_[(size_t)w * 2 + 1] = R ? R[i] : L[i];
            w = (w + 1) % n_;
        }
        w_.store(w, std::memory_order_release);
    }
    // consumer: pop up to frames into planar L/R, zero-fills shortfall; returns frames read
    int pop(float* L, float* R, int frames) {
        int r = r_.load(std::memory_order_relaxed);
        int w = w_.load(std::memory_order_acquire);
        int avail = (w - r + n_) % n_;
        int take = frames < avail ? frames : avail;
        for (int i = 0; i < take; i++) {
            L[i] = buf_[(size_t)r * 2];
            R[i] = buf_[(size_t)r * 2 + 1];
            r = (r + 1) % n_;
        }
        for (int i = take; i < frames; i++) { L[i] = 0; R[i] = 0; }
        r_.store(r, std::memory_order_release);
        return take;
    }
    // consumer: drop the oldest `frames` (latency control)
    void discard(int frames) {
        int r = r_.load(std::memory_order_relaxed);
        int w = w_.load(std::memory_order_acquire);
        int avail = (w - r + n_) % n_;
        if (frames > avail) frames = avail;
        r_.store((r + frames) % n_, std::memory_order_release);
    }
    int available() const {
        return (w_.load(std::memory_order_acquire) - r_.load(std::memory_order_acquire) + n_) % n_;
    }
private:
    std::vector<float> buf_;
    int n_ = 0;
    std::atomic<int> r_{0}, w_{0};
};
