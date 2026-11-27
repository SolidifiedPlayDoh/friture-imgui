#pragma once
// Scrolling time/frequency image (port of friture/spectrogram.py), drawn from a
// GPU texture ring.

#include <cstdint>
#include <vector>

#include "dsp/audioproc.hpp"
#include "dsp/scales.hpp"
#include "ui/plot_util.hpp"
#include "widgets/widget.hpp"

namespace fr {

class SpectrogramWidget : public Widget {
public:
    explicit SpectrogramWidget(Context& ctx);
    ~SpectrogramWidget() override;
    const char* name() const override { return "2D Spectrogram"; }
    void onData(const Chunk& chunk) override;
    void draw(ImVec2 size) override;
    void drawSettings() override;
    void save(Settings& s, const std::string& p) const override;
    void load(const Settings& s, const std::string& p) override;
    void onStreamReset() override;

private:
    void rebuild();           // FFT size / time range / frequency mapping changed
    void rebuildTexture();
    void pushColumn(const std::vector<double>& norm);
    void uploadColumn(const std::vector<double>& rows);

    AudioProc proc_;
    int fftIndex_ = 7;  // 4096 points
    FreqScale scale_ = FreqScale::Mel;
    int minFreq_ = 20, maxFreq_ = 24000;
    int specMin_ = -140, specMax_ = 0;
    double timeRange_ = 10.0;
    int weighting_ = 0;

    int fftSize_ = 4096;
    double overlap_ = 0.75;
    int64_t oldIndex_ = 0;
    std::vector<double> w_, sp_, in_, norm_, rowFreq_, rows_, acc_;
    int mergeK_ = 1, accCount_ = 0;

    unsigned tex_ = 0;
    int texW_ = 0, head_ = 0;
    static constexpr int kTexH = 1024;
    std::vector<uint32_t> column_;
    FreqAxis yaxis_;
};

}  // namespace fr
