#include "dsp/audioproc.hpp"

#include <cmath>

#include "config.hpp"
#include "dsp/fft.hpp"

namespace fr {

void AudioProc::setFftSize(int n) {
    if (n == n_) return;
    n_ = n;

    window_.resize(n);
    for (int i = 0; i < n; ++i)
        window_[i] = 0.5 * (1.0 - std::cos(2.0 * kPi * i / (n - 1)));

    int bins = n / 2 + 1;
    freq_.resize(bins);
    A_.resize(bins);
    B_.resize(bins);
    C_.resize(bins);
    const double fmax = kSampleRate / 2;
    for (int i = 0; i < bins; ++i) {
        double f = bins > 1 ? fmax * i / (bins - 1) : 0.0;
        freq_[i] = f;
        double f2 = f * f;
        double Rc = 12200.0 * 12200.0 * f2 / ((f2 + 20.6 * 20.6) * (f2 + 12200.0 * 12200.0));
        double Rb = 12200.0 * 12200.0 * f * f2 /
                    ((f2 + 20.6 * 20.6) * (f2 + 12200.0 * 12200.0) * std::sqrt(f2 + 158.5 * 158.5));
        double Ra = 12200.0 * 12200.0 * f2 * f2 /
                    ((f2 + 20.6 * 20.6) * (f2 + 12200.0 * 12200.0) *
                     std::sqrt(f2 + 107.7 * 107.7) * std::sqrt(f2 + 737.9 * 737.9));
        const double eps = 1e-50;
        C_[i] = 0.06 + 20.0 * std::log10(Rc + eps);
        B_[i] = 0.17 + 20.0 * std::log10(Rb + eps);
        A_[i] = 2.0 + 20.0 * std::log10(Ra + eps);
    }
    scratchIn_.assign(n, 0.0);
    scratchOut_.assign(bins, {});
}

void AudioProc::analyze(const double* samples, std::vector<double>& power) {
    for (int i = 0; i < n_; ++i) scratchIn_[i] = samples[i] * window_[i];
    rfft(scratchIn_.data(), (size_t)n_, scratchOut_.data());
    const double sq = (double)n_ * (double)n_;
    power.resize(scratchOut_.size());
    for (size_t i = 0; i < scratchOut_.size(); ++i)
        power[i] = std::norm(scratchOut_[i]) / sq;
}

std::vector<double> AudioProc::weighting(int kind) const {
    switch (kind) {
        case 1: return A_;
        case 2: return B_;
        case 3: return C_;
        default: return std::vector<double>(freq_.size(), 0.0);
    }
}

}  // namespace fr
