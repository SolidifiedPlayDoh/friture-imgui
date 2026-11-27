#include "widgets/spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config.hpp"
#include "ui/controls.hpp"

namespace fr {

namespace {

constexpr int kFftSizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
const char* const kFftNames[] = {"32 points",   "64 points",   "128 points", "256 points",
                                 "512 points",  "1024 points", "2048 points", "4096 points",
                                 "8192 points", "16384 points"};
const char* const kWeightNames[] = {"None", "A", "B", "C"};
const char* const kResponseNames[] = {"25 ms (Impulse)", "125 ms (Fast)", "300 ms", "1s (Slow)",
                                      "5s (Very Slow)"};
constexpr double kResponseSecs[] = {0.025, 0.125, 0.3, 1.0, 5.0};
const char* const kScaleNames[] = {"Linear", "Logarithmic", "Mel", "ERB", "Octave"};

constexpr double kPeakDecayRate = 1.0 - 3e-6;

}  // namespace

SpectrumWidget::SpectrumWidget(Context& ctx) : Widget(ctx) { rebuild(); }

void SpectrumWidget::rebuild() {
    fftSize_ = kFftSizes[std::clamp(fftIndex_, 0, 9)];
    proc_.setFftSize(fftSize_);
    const size_t bins = proc_.freq().size();
    disp1_.assign(bins, 0.0);
    disp2_.assign(bins, 0.0);
    w_ = proc_.weighting(weighting_);
    peak_.clear();
    ctx_.ring.growIfNeeded((size_t)fftSize_ * 4);
    oldIndex_ = ctx_.ring.offset();
    updateResponse();
}

void SpectrumWidget::updateResponse() {
    const double w = 0.65;
    double deltaN = fftSize_ * (1.0 - overlap_);
    double n = kResponseSecs[std::clamp(responseIndex_, 0, 4)] * kSampleRate / deltaN;
    alpha_ = 1.0 - std::pow(1.0 - w, 1.0 / (n + 1.0));
}

void SpectrumWidget::onStreamReset() { oldIndex_ = ctx_.ring.offset(); }

void SpectrumWidget::onData(const Chunk&) {
    const int64_t index = ctx_.ring.offset();
    int64_t available = index - oldIndex_;
    if (available < 0) {
        available = 0;
        oldIndex_ = index;
    }
    const int needed = (int)(fftSize_ * (1.0 - overlap_));
    int64_t realizable = available / needed;
    if (realizable <= 0) return;
    if (realizable > 64) {  // fell far behind: skip ahead rather than stall the UI
        oldIndex_ += (realizable - 64) * needed;
        realizable = 64;
    }

    const size_t bins = proc_.freq().size();
    const bool dual = dual_ && ctx_.ring.channels() > 1;
    buf1_.resize(fftSize_);
    buf2_.resize(fftSize_);
    for (int64_t i = 0; i < realizable; ++i) {
        ctx_.ring.copy(0, oldIndex_, fftSize_, buf1_.data());
        proc_.analyze(buf1_.data(), sp_);
        for (size_t k = 0; k < bins; ++k) disp1_[k] = alpha_ * sp_[k] + (1.0 - alpha_) * disp1_[k];
        if (dual) {
            ctx_.ring.copy(1, oldIndex_, fftSize_, buf2_.data());
            proc_.analyze(buf2_.data(), sp_);
            for (size_t k = 0; k < bins; ++k)
                disp2_[k] = alpha_ * sp_[k] + (1.0 - alpha_) * disp2_[k];
        } else {
            for (size_t k = 0; k < bins; ++k) disp2_[k] = (1.0 - alpha_) * disp2_[k];
        }
        oldIndex_ += needed;
    }

    const double eps = 1e-30;
    dB_.resize(bins);
    if (dual) {
        for (size_t k = 0; k < bins; ++k)
            dB_[k] = 10.0 * std::log10(disp2_[k] + eps) - 10.0 * std::log10(disp1_[k] + eps);
    } else {
        for (size_t k = 0; k < bins; ++k)
            dB_[k] = 10.0 * std::log10(disp1_[k] + eps) + w_[k];
    }

    size_t imax = std::max_element(dB_.begin(), dB_.end()) - dB_.begin();
    fmax_ = proc_.freq()[imax];

    // Harmonic product spectrum over 3 harmonics, for the pitch label.
    size_t hl = bins / 3;
    double best = -1;
    size_t bi = 0;
    for (size_t k = 0; k < hl; ++k) {
        double v = disp1_[k] * disp1_[2 * k < bins ? 2 * k : bins - 1] *
                   disp1_[3 * k < bins ? 3 * k : bins - 1];
        if (v > best) { best = v; bi = k; }
    }
    fpitch_ = std::max(proc_.freq()[bi], 1e-20);

    if (!dual) computePeaks(dB_);
}

void SpectrumWidget::computePeaks(const std::vector<double>& y) {
    if (peak_.size() != y.size()) {
        peak_.assign(y.size(), -500.0);
        peakInt_.assign(y.size(), 0.0);
        peakDecay_.assign(y.size(), 20.0 * std::log10(kPeakDecayRate) * 5000);
    }
    const double step = 20.0 * std::log10(kPeakDecayRate) * 5000;
    for (size_t i = 0; i < y.size(); ++i) {
        if (peak_[i] < y[i]) {
            peak_[i] = y[i];
            peakDecay_[i] = step;
            peakInt_[i] = 1.0;
        } else if (peakInt_[i] < 0.2) {
            peak_[i] += peakDecay_[i];
            peakDecay_[i] += step;
        } else {
            peakInt_[i] *= 0.975;
        }
    }
}

void SpectrumWidget::draw(ImVec2 size) {
    const std::vector<double>& freq = proc_.freq();
    const bool dual = dual_ && ctx_.ring.channels() > 1;
    const double lo = std::min(minFreq_, maxFreq_), hi = std::max(minFreq_, maxFreq_);

    if (!ImPlot::BeginPlot("##spectrum", size, kPlotFlags)) return;
    xaxis_.setup(ImAxis_X1, scale_, lo, hi, "Frequency (Hz)", showFreqLabels_);
    char ylabel[48];
    if (dual) std::snprintf(ylabel, sizeof ylabel, "Ch2 - Ch1 (dB)");
    else if (weighting_ > 0) std::snprintf(ylabel, sizeof ylabel, "PSD (dB %s)", kWeightNames[weighting_]);
    else std::snprintf(ylabel, sizeof ylabel, "PSD (dB)");
    setupValueAxis(ImAxis_Y1, specMin_, specMax_, ylabel);
    ImPlot::SetupFinish();
    xaxis_.drawMinorGrid();

    if (!dB_.empty() && dB_.size() == freq.size()) {
        // Visible bin range, with one bin of margin either side; skip DC.
        size_t i0 = std::max<size_t>(1, (size_t)(std::lower_bound(freq.begin(), freq.end(), lo) - freq.begin()));
        size_t i1 = (size_t)(std::upper_bound(freq.begin(), freq.end(), hi) - freq.begin());
        if (i0 > 1) --i0;
        i1 = std::min(freq.size(), i1 + 1);
        if (i1 > i0 + 1) {
            decimateIndices(freq.data(), dB_.data(), i0, i1, scale_, lo, hi, (int)size.x, idx_);
            const int n = (int)idx_.size();
            xs_.resize(n);
            ys_.resize(n);
            for (int k = 0; k < n; ++k) {
                xs_[k] = freq[idx_[k]];
                ys_[k] = dB_[idx_[k]];
            }
            const double baseline = dual ? 0.0 : specMin_;

            ImPlotSpec fill;
            fill.FillColor = rgb(palette::kTeal);
            fill.FillAlpha = 0.22f;
            fill.LineColor = rgb(palette::kTeal, 0.f);
            fill.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
            ImPlot::PlotShaded("##fill", xs_.data(), ys_.data(), n, baseline, fill);

            if (!dual && peak_.size() == dB_.size()) {
                peakColors_.resize(n);
                ps_.resize(n);
                for (int k = 0; k < n; ++k) {
                    ps_[k] = peak_[idx_[k]];
                    float a = (float)std::clamp(peakInt_[idx_[k]], 0.0, 1.0) * 0.55f;
                    peakColors_[k] = IM_COL32(90, 169, 255, (int)(a * 255));
                }
                ImPlotSpec pspec;
                pspec.LineWeight = 1.f;
                pspec.LineColors = peakColors_.data();
                pspec.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
                ImPlot::PlotLine("##peaks", xs_.data(), ps_.data(), n, pspec);
            }

            ImPlotSpec line;
            line.LineColor = rgb(palette::kTeal);
            line.LineWeight = 1.5f;
            line.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
            ImPlot::PlotLine("##curve", xs_.data(), ys_.data(), n, line);
        }

        // Peak-frequency and pitch markers.
        ImPlotSpec mark;
        mark.LineColor = rgb(palette::kAmber, 0.6f);
        mark.LineWeight = 1.f;
        mark.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
        if (showFreqLabels_ && fmax_ >= lo && fmax_ <= hi) {
            ImPlot::PlotInfLines("##fmax", &fmax_, 1, mark);
            char t[48];
            if (fmax_ < 200) std::snprintf(t, sizeof t, "%.1f Hz", fmax_);
            else std::snprintf(t, sizeof t, "%d Hz", (int)std::lround(fmax_));
            ImPlot::PlotText(t, fmax_, specMax_ - (specMax_ - specMin_) * 0.04,
                             ImVec2(6, 0));
        }
        if (showPitchLabels_ && !dual && fpitch_ >= lo && fpitch_ <= hi) {
            ImPlotSpec pm = mark;
            pm.LineColor = rgb(palette::kBlue, 0.5f);
            ImPlot::PlotInfLines("##fpitch", &fpitch_, 1, pm);
            ImPlot::PlotText(formatFrequency(fpitch_).c_str(), fpitch_,
                             specMax_ - (specMax_ - specMin_) * 0.10, ImVec2(6, 0));
        }

        // Hover tracker.
        ImPlotPoint mp;
        if (plotHovered(&mp) && mp.x > 0) {
            size_t k = (size_t)(std::lower_bound(freq.begin(), freq.end(), mp.x) - freq.begin());
            k = std::min(k, freq.size() - 1);
            ImGui::BeginTooltip();
            ImGui::Text("%s Hz", formatAxisFreq(freq[k]).c_str());
            ImGui::Text("%.1f dB", dB_[k]);
            ImGui::EndTooltip();
        }
    }
    ImPlot::EndPlot();
}

void SpectrumWidget::drawSettings() {
    bool rb = false;
    int dual = dual_ ? 1 : 0;
    const char* const types[] = {"Single-channel", "Dual-channel"};
    if (ui::combo("Measurement type", dual, types, 2)) {
        dual_ = dual == 1;
        peak_.clear();
        std::fill(disp1_.begin(), disp1_.end(), 0.0);
        std::fill(disp2_.begin(), disp2_.end(), 0.0);
    }
    if (ui::combo("FFT size", fftIndex_, kFftNames, 10)) rb = true;
    int sc = (int)scale_;
    if (ui::combo("Frequency scale", sc, kScaleNames, kUserScaleCount)) scale_ = (FreqScale)sc;
    ui::spinInt("Min frequency", minFreq_, 20, kSampleRate / 2, " Hz", 10.f);
    ui::spinInt("Max frequency", maxFreq_, 20, kSampleRate / 2, " Hz", 100.f);
    ui::spinInt("Min", specMin_, -200, 200, " dB");
    ui::spinInt("Max", specMax_, -200, 200, " dB");
    if (ui::combo("Middle-ear weighting", weighting_, kWeightNames, 4)) w_ = proc_.weighting(weighting_);
    if (ui::combo("Response time", responseIndex_, kResponseNames, 5)) updateResponse();
    ui::check("Show frequency labels", showFreqLabels_);
    ui::check("Show pitch labels", showPitchLabels_);
    if (rb) rebuild();
}

void SpectrumWidget::save(Settings& s, const std::string& p) const {
    s.set(p + "fft", fftIndex_);
    s.set(p + "scale", (int)scale_);
    s.set(p + "minfreq", minFreq_);
    s.set(p + "maxfreq", maxFreq_);
    s.set(p + "specmin", specMin_);
    s.set(p + "specmax", specMax_);
    s.set(p + "weighting", weighting_);
    s.set(p + "response", responseIndex_);
    s.set(p + "dual", dual_);
    s.set(p + "freqlabels", showFreqLabels_);
    s.set(p + "pitchlabels", showPitchLabels_);
}

void SpectrumWidget::load(const Settings& s, const std::string& p) {
    fftIndex_ = s.getInt(p + "fft", fftIndex_);
    scale_ = (FreqScale)std::clamp(s.getInt(p + "scale", (int)scale_), 0, kUserScaleCount - 1);
    minFreq_ = s.getInt(p + "minfreq", minFreq_);
    maxFreq_ = s.getInt(p + "maxfreq", maxFreq_);
    specMin_ = s.getInt(p + "specmin", specMin_);
    specMax_ = s.getInt(p + "specmax", specMax_);
    weighting_ = std::clamp(s.getInt(p + "weighting", weighting_), 0, 3);
    responseIndex_ = std::clamp(s.getInt(p + "response", responseIndex_), 0, 4);
    dual_ = s.getBool(p + "dual", dual_);
    showFreqLabels_ = s.getBool(p + "freqlabels", showFreqLabels_);
    showPitchLabels_ = s.getBool(p + "pitchlabels", showPitchLabels_);
    rebuild();
}

}  // namespace fr
