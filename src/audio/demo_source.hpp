#pragma once
// Built-in synthetic input, so the analyzers can be explored (and tested)
// without a microphone: a gliding harmonic tone, a tremolo sine and a little
// noise. Channel 2 is channel 1 delayed by 1.5 ms (handy for the delay estimator).

#include <cmath>
#include <cstdint>
#include <vector>

#include "config.hpp"

namespace fr {

class DemoSource {
public:
    static constexpr int kDelaySamples = 72;  // 1.5 ms at 48 kHz

    void generate(size_t n, std::vector<float>& a, std::vector<float>& b) {
        a.resize(n);
        b.resize(n);
        for (size_t i = 0; i < n; ++i) {
            double t = (double)count_ / kSampleRate;
            double f = 220.0 * std::pow(2.0, 0.5 * std::sin(2.0 * kPi * t / 6.0));  // +-6 semitones
            phase_ += 2.0 * kPi * f / kSampleRate;
            if (phase_ > 2.0 * kPi * 1024.0) phase_ = std::fmod(phase_, 2.0 * kPi);
            double s = 0.0;
            for (int h = 1; h <= 6; ++h) s += std::sin(h * phase_) / h;
            s *= 0.22;
            s += 0.12 * std::sin(2.0 * kPi * 1000.0 * t) * (0.5 + 0.5 * std::sin(2.0 * kPi * 0.5 * t));
            s += 0.03 * noise(count_);
            hist_[(size_t)(count_ & 255)] = (float)s;
            a[i] = (float)s;
            b[i] = count_ >= kDelaySamples ? hist_[(size_t)((count_ - kDelaySamples) & 255)] : 0.f;
            ++count_;
        }
    }

private:
    static double noise(int64_t i) {  // deterministic white noise in [-1, 1)
        uint64_t z = (uint64_t)i + 0x9E3779B97F4A7C15ULL;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        z ^= z >> 31;
        return (double)(z >> 11) / (double)(1ULL << 52) - 1.0;
    }
    int64_t count_ = 0;
    double phase_ = 0.0;
    float hist_[256] = {};
};

}  // namespace fr
