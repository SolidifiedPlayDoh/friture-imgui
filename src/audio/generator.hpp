#pragma once
// Test-signal generator: sine, white noise, pink noise, log sweep, burst
// (port of friture/generator.py and friture/generators/*).
// Runs on the audio thread; parameters are atomics set from the UI.

#include <atomic>
#include <random>
#include <vector>

#include "audio/engine.hpp"

namespace fr {

enum class GeneratorKind { Sine = 0, White, Pink, Sweep, Burst, Count };

class SignalGenerator : public OutputSource {
public:
    SignalGenerator();

    static const char* kindName(int kind);

    // ---- control (UI thread)
    std::atomic<int> kind{0};
    std::atomic<double> sineFrequency{440.0};
    std::atomic<double> sweepStart{20.0};
    std::atomic<double> sweepStop{22000.0};
    std::atomic<double> sweepPeriod{1.0};
    std::atomic<double> burstPeriod{1.0};
    std::atomic<double> volumeDb{-12.0};  // added safety control (upstream plays at full scale)

    void setPlaying(bool on) { want_.store(on); }
    bool isPlaying() const { return want_.load(); }

    // ---- OutputSource
    void mixInto(float* interleaved, uint32_t frames, uint32_t channels) override;
    bool wantsOutput() const override { return want_.load() || state_.load() != kStopped; }

private:
    enum State { kStopped = 0, kStarting, kPlaying, kStopping };

    void signal(const double* t, double* out, size_t n);
    void sweepParams(double f1, double f2, double T, double& L, double& K, double& Tadj) const;

    std::atomic<bool> want_{false};
    std::atomic<int> state_{kStopped};

    // audio-thread state
    double t_ = 0.0, tStart_ = 0.0, tStop_ = 0.0;
    std::vector<double> tbuf_, sbuf_, nbuf_;
    std::mt19937 rng_{12345};
    std::normal_distribution<double> normal_{0.0, 1.0};

    // sine phase continuity
    double sineFreq_ = 440.0, sineOffset_ = 0.0, sineLastT_ = 0.0;

    // sweep
    double sweepL_ = 0, sweepK_ = 0, sweepT_ = 1, sweepTimeOffset_ = 0;
    bool sweepPending_ = false;
    double nextL_ = 0, nextK_ = 0, nextT_ = 1;
    double lastF1_ = 0, lastF2_ = 0, lastPeriod_ = 0;
    bool sweepInit_ = false;
};

}  // namespace fr
