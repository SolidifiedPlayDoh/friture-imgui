#include "audio/generator.hpp"

#include <algorithm>
#include <cmath>

#include "config.hpp"

namespace fr {

namespace {
constexpr double kRamp = 3e-3;  // start/stop ramp, seconds
constexpr int kPinkOctaves = 13;
}  // namespace

SignalGenerator::SignalGenerator() = default;

const char* SignalGenerator::kindName(int kind) {
    static const char* names[] = {"Sine", "White noise", "Pink noise", "Sweep", "Burst"};
    return (kind >= 0 && kind < 5) ? names[kind] : "";
}

void SignalGenerator::sweepParams(double f1, double f2, double T, double& L, double& K,
                                  double& Tadj) const {
    f1 = std::max(f1, 1.0);
    f2 = std::max(f2, f1 * 1.001);
    double Tmult = std::log(f2 / f1) / (f1 * (f2 / f1 - 1.0));
    if (T >= Tmult) T = std::round(T / Tmult) * Tmult;
    else T = std::ceil(T / Tmult) * Tmult;
    double w1 = 2.0 * kPi * f1, w2 = 2.0 * kPi * f2;
    K = w1 * T / std::log(w2 / w1);
    L = T / std::log(w2 / w1);
    Tadj = T;
}

void SignalGenerator::signal(const double* t, double* out, size_t n) {
    const int k = kind.load();
    switch (k) {
        case (int)GeneratorKind::Sine: {
            double f = sineFrequency.load();
            if (f != sineFreq_) {  // keep the phase continuous across a frequency change
                double lastPhase = 2.0 * kPi * sineLastT_ * sineFreq_ + sineOffset_;
                double newPhase = 2.0 * kPi * sineLastT_ * f + sineOffset_;
                sineOffset_ = std::fmod(sineOffset_ + (lastPhase - newPhase), 2.0 * kPi);
                sineFreq_ = f;
            }
            for (size_t i = 0; i < n; ++i) out[i] = std::sin(2.0 * kPi * t[i] * sineFreq_ + sineOffset_);
            if (n) sineLastT_ = t[n - 1];
            break;
        }
        case (int)GeneratorKind::White:
            for (size_t i = 0; i < n; ++i) out[i] = normal_(rng_);
            break;
        case (int)GeneratorKind::Pink: {
            std::fill(out, out + n, 0.0);
            for (int o = 0; o < kPinkOctaves; ++o) {
                size_t m = (size_t)1 << o;
                double v = 0;
                for (size_t i = 0; i < n; ++i) {
                    if (i % m == 0) v = normal_(rng_);
                    out[i] += v;
                }
            }
            for (size_t i = 0; i < n; ++i) out[i] /= kPinkOctaves;
            break;
        }
        case (int)GeneratorKind::Sweep: {
            double f1 = sweepStart.load(), f2 = sweepStop.load(), per = sweepPeriod.load();
            if (!sweepInit_) {
                sweepParams(f1, f2, per, sweepL_, sweepK_, sweepT_);
                lastF1_ = f1; lastF2_ = f2; lastPeriod_ = per;
                sweepInit_ = true;
            } else if (f1 != lastF1_ || f2 != lastF2_ || per != lastPeriod_) {
                sweepParams(f1, f2, per, nextL_, nextK_, nextT_);
                sweepPending_ = true;
                lastF1_ = f1; lastF2_ = f2; lastPeriod_ = per;
            }
            auto eval = [&](double tt) {
                double ph = std::fmod(tt - sweepTimeOffset_, sweepT_);
                if (ph < 0) ph += sweepT_;
                return std::cos(sweepK_ * (std::exp(ph / sweepL_) - 1.0));
            };
            for (size_t i = 0; i < n; ++i) out[i] = eval(t[i]);
            if (sweepPending_ && n > 3) {
                // switch parameters at the first local maximum to avoid a click
                for (size_t i = 0; i + 2 < n; ++i) {
                    double d0 = out[i + 1] - out[i], d1 = out[i + 2] - out[i + 1];
                    if (d0 > 0 && d1 < 0) {
                        size_t first = i + 1;
                        sweepL_ = nextL_; sweepK_ = nextK_; sweepT_ = nextT_;
                        sweepPending_ = false;
                        sweepTimeOffset_ = t[first];
                        for (size_t j = first; j < n; ++j) out[j] = eval(t[j]);
                        break;
                    }
                }
            }
            break;
        }
        case (int)GeneratorKind::Burst: {
            double period = std::max(burstPeriod.load(), 1.0 / kSampleRate);
            double ps = period * kSampleRate;
            for (size_t i = 0; i < n; ++i) {
                double idx = std::fmod(t[i] * kSampleRate, ps);
                out[i] = idx < 1.0 ? 1.0 : 0.0;
            }
            break;
        }
        default: std::fill(out, out + n, 0.0);
    }
}

void SignalGenerator::mixInto(float* out, uint32_t frames, uint32_t channels) {
    int st = state_.load();
    if (st == kStopped) {
        if (!want_.load()) return;
        st = kStarting;
        tStart_ = 0.0;
        state_.store(st);
    } else if ((st == kPlaying || st == kStarting) && !want_.load()) {
        st = kStopping;
        tStop_ = kRamp;
        state_.store(st);
    } else if (st == kStopping && want_.load()) {
        st = kStarting;
        tStart_ = 0.0;
        state_.store(st);
    }
    if (frames == 0) return;

    const double dt = 1.0 / kSampleRate;
    tbuf_.resize(frames);
    sbuf_.resize(frames);
    for (uint32_t i = 0; i < frames; ++i) tbuf_[i] = t_ + i * dt;
    signal(tbuf_.data(), sbuf_.data(), frames);

    if (st == kStarting) {
        for (uint32_t i = 0; i < frames; ++i) {
            double tr = std::clamp(tStart_ + i * dt, 0.0, kRamp);
            sbuf_[i] *= tr / kRamp;
        }
        tStart_ += frames * dt;
        if (tStart_ > kRamp) state_.store(kPlaying);
    } else if (st == kStopping) {
        for (uint32_t i = 0; i < frames; ++i) {
            double tr = std::clamp(tStop_ - i * dt, 0.0, kRamp);
            sbuf_[i] *= tr / kRamp;
        }
        tStop_ -= frames * dt;
        if (tStop_ < 0.0) state_.store(kStopped);
    }
    t_ += frames * dt;

    const float gain = (float)std::pow(10.0, volumeDb.load() / 20.0);
    for (uint32_t i = 0; i < frames; ++i) {
        float v = (float)std::clamp(sbuf_[i], -1.0, 1.0) * gain;
        for (uint32_t c = 0; c < channels; ++c) out[(size_t)i * channels + c] += v;
    }
}

}  // namespace fr
