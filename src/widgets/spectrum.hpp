#pragma once
// FFT spectrum analyzer (port of friture/spectrum.py + spectrumPlotWidget.py).

#include <vector>

#include "dsp/audioproc.hpp"
#include "dsp/scales.hpp"
#include "ui/plot_util.hpp"
#include "widgets/widget.hpp"

namespace fr {

class SpectrumWidget : public Widget {
public:
    explicit SpectrumWidget(Context& ctx);
    const char* name() const override { return "FFT Spectrum"; }
    void onData(const Chunk& chunk) override;
    void draw(ImVec2 size) override;
    void drawSettings() override;
    void save(Settings& s, const std::string& p) const override;
    void load(const Settings& s, const std::string& p) override;
    void onStreamReset() override;

private:
    void rebuild();            // after an fft size / range change
    void updateResponse();
    void computePeaks(const std::vector<double>& y);

    AudioProc proc_;
    // settings
    int fftIndex_ = 8;          // 8192 points
    FreqScale scale_ = FreqScale::Mel;
    int minFreq_ = 20, maxFreq_ = 20000;
    int specMin_ = -100, specMax_ = -20;
    int weighting_ = 1;         // A
    int responseIndex_ = 0;     // 25 ms
    bool dual_ = false;
    bool showFreqLabels_ = true, showPitchLabels_ = true;

    int fftSize_ = 8192;
    double overlap_ = 0.75;
    double alpha_ = 0.1;
    int64_t oldIndex_ = 0;

    std::vector<double> disp1_, disp2_, w_;
    std::vector<double> sp_, buf1_, buf2_, tmp_, dB_;
    double fmax_ = 0, fpitch_ = 0;

    // peak hold
    std::vector<double> peak_, peakInt_, peakDecay_;
    std::vector<unsigned> peakColors_;
    std::vector<int> idx_;
    std::vector<double> xs_, ys_, ps_;

    FreqAxis xaxis_;
};

}  // namespace fr
