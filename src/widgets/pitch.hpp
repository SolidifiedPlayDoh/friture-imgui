#pragma once
// Pitch tracker (port of friture/pitch_tracker.py). The estimator is adapted from libf0
// (MIT) and based on the SWIPE algorithm by Arturo Camacho.

#include <deque>
#include <vector>

#include "dsp/audioproc.hpp"
#include "ui/plot_util.hpp"
#include "widgets/widget.hpp"

namespace fr {

class PitchWidget : public Widget {
public:
    explicit PitchWidget(Context& ctx);
    const char* name() const override { return "Pitch Tracker"; }
    void onData(const Chunk& chunk) override;
    void draw(ImVec2 size) override;
    void drawSettings() override;
    void save(Settings& s, const std::string& p) const override;
    void load(const Settings& s, const std::string& p) override;
    void onStreamReset() override;

private:
    void initSwipe();
    double estimate(const std::vector<double>& frame);

    AudioProc proc_;
    int fftSize_ = 4096;
    double overlap_ = 0.75;
    int minFreq_ = 65, maxFreq_ = 1047;
    int duration_ = 10;
    double minDb_ = -50.0;
    int cres_ = 10;
    double conf_ = 0.5;
    int pDelta_ = 2;

    std::vector<double> logFreqs_;
    std::vector<double> kernels_;  // [candidate][logFreq]
    int nCand_ = 0;
    double prevF0_ = 0.0;
    bool hasPrev_ = false;

    int64_t nextIn_ = 0;
    std::deque<double> estimates_;
    std::vector<double> frame_, spec_, specLog_, strengths_, xs_, ys_;
    FreqAxis yaxis_;
};

}  // namespace fr
