#include "widgets/pitch.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>

#include "config.hpp"
#include "dsp/fft.hpp"
#include "dsp/scales.hpp"
#include "ui/controls.hpp"

namespace fr {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

// SWIPE-style kernel for candidate f over the given log-spaced frequencies.
void cosineKernel(double f, const std::vector<double>& freqs, double* k) {
    static const int harmonics[] = {1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 13, 17, 19, 23};
    constexpr int kNumH = 14;
    const double peakWidth = 0.15;
    const double valleyWidth = 1.0 - peakWidth;
    const double valleyDivisor = 2.0;
    const double decayStartHarmonic = 2.0;

    int maxPossible = (int)(freqs.back() / f);
    maxPossible = std::min(maxPossible, kNumH);
    auto selected = [&](int i) {
        for (int h = 0; h < maxPossible; ++h)
            if (harmonics[h] == i) return true;
        return false;
    };
    const double harmonicsNormalizer = (double)maxPossible / (double)kNumH;

    const size_t n = freqs.size();
    std::fill(k, k + n, 0.0);
    for (int i = 1; i <= harmonics[kNumH - 1]; ++i) {
        const bool sel = selected(i);
        for (size_t j = 0; j < n; ++j) {
            double a = freqs[j] / f - i;
            if (sel) {
                if (-valleyWidth < a && a < -peakWidth)
                    k[j] = -std::cos((a + 0.5) / ((valleyWidth - peakWidth) / 2.0) * (kPi / 2.0)) / valleyDivisor;
                if (std::fabs(a) < peakWidth) k[j] = std::cos(a / peakWidth * (kPi / 2.0));
            } else {
                if (-valleyWidth < a && a < peakWidth)
                    k[j] = -std::cos((a + 0.5) / ((valleyWidth - peakWidth) / 2.0) * (kPi / 2.0)) / valleyDivisor;
                if (std::fabs(a) < peakWidth) k[j] = std::cos(a / peakWidth * (kPi / 2.0)) / 4.0;
            }
        }
    }
    const double f2 = f * (decayStartHarmonic + peakWidth);
    const double ref = std::sqrt(1.0 / f2);
    double pos = 0.0;
    for (size_t j = 0; j < n; ++j) {
        double decay = (freqs[j] <= f2 ? std::sqrt(1.0 / f2) : std::sqrt(1.0 / freqs[j])) / ref;
        k[j] *= decay;
    }
    for (size_t j = 0; j < n; ++j) if (k[j] > 0) pos += k[j];
    if (pos > 0) for (size_t j = 0; j < n; ++j) k[j] /= pos;
    for (size_t j = 0; j < n; ++j) k[j] /= harmonicsNormalizer;
}

std::string noteLabel(double f) { return frequencyToNote(f); }

}  // namespace

PitchWidget::PitchWidget(Context& ctx) : Widget(ctx) {
    proc_.setFftSize(fftSize_);
    ctx_.ring.growIfNeeded((size_t)fftSize_ * 4);
    nextIn_ = ctx_.ring.offset();
    initSwipe();
}

void PitchWidget::onStreamReset() { nextIn_ = ctx_.ring.offset(); hasPrev_ = false; }

void PitchWidget::initSwipe() {
    const double fs = kSampleRate;
    int n = (int)(std::log2(fs / (2.0 * minFreq_)) * (1200.0 / cres_));
    n = std::max(n, 8);
    const double l0 = std::log2((double)minFreq_), l1 = std::log2((double)(kSampleRate / 2));
    logFreqs_.resize(n);
    for (int i = 0; i < n; ++i) logFreqs_[i] = std::pow(2.0, l0 + (l1 - l0) * i / (double)(n - 1));
    nCand_ = (int)(std::lower_bound(logFreqs_.begin(), logFreqs_.end(), (double)maxFreq_) - logFreqs_.begin());
    nCand_ = std::max(nCand_, 3);
    kernels_.assign((size_t)nCand_ * n, 0.0);
    for (int c = 0; c < nCand_; ++c) cosineKernel(logFreqs_[c], logFreqs_, &kernels_[(size_t)c * n]);
}

double PitchWidget::estimate(const std::vector<double>& frame) {
    const int n = fftSize_;
    std::vector<double> in(n);
    for (int i = 0; i < n; ++i) in[i] = frame[i] * proc_.window()[i];
    std::vector<std::complex<double>> out(n / 2 + 1);
    rfft(in.data(), (size_t)n, out.data());
    spec_.resize(out.size());
    for (size_t i = 0; i < out.size(); ++i) spec_[i] = std::abs(out[i]);

    // Resample the linear-frequency magnitude spectrum onto the log-spaced grid.
    const size_t F = logFreqs_.size();
    specLog_.resize(F);
    const double binW = (double)kSampleRate / (double)n;
    double ms = 0;
    for (size_t j = 0; j < F; ++j) {
        double pos = logFreqs_[j] / binW;
        double v;
        if (pos <= 0) v = spec_[0];
        else if (pos >= (double)(spec_.size() - 1)) v = spec_.back();
        else {
            size_t i = (size_t)pos;
            double fr_ = pos - (double)i;
            v = spec_[i] * (1.0 - fr_) + spec_[i + 1] * fr_;
        }
        specLog_[j] = v;
        ms += v * v;
    }
    const double rms = std::sqrt(ms / (double)F);
    for (size_t j = 0; j < F; ++j) specLog_[j] /= rms;

    strengths_.assign(nCand_, 0.0);
    int idxMax = 0;
    for (int c = 0; c < nCand_; ++c) {
        const double* k = &kernels_[(size_t)c * F];
        double s = 0;
        for (size_t j = 0; j < F; ++j) s += k[j] * specLog_[j];
        strengths_[c] = s;
        if (s > strengths_[idxMax]) idxMax = c;
    }

    double shift = 0;
    if (idxMax > 0 && idxMax < nCand_ - 1) {
        double y1 = strengths_[idxMax - 1], y2 = strengths_[idxMax], y3 = strengths_[idxMax + 1];
        double a = (y1 - 2 * y2 + y3) / 2, b = (y3 - y1) / 2;
        shift = -b / (2 * a + std::numeric_limits<double>::epsilon());
    }
    // f0 = interp(idxMax + shift, arange(len), logFreqs)
    double pos = std::clamp((double)idxMax + shift, 0.0, (double)(F - 1));
    size_t i0 = (size_t)pos;
    double f0 = (i0 + 1 < F) ? logFreqs_[i0] * (1.0 - (pos - (double)i0)) + logFreqs_[i0 + 1] * (pos - (double)i0)
                             : logFreqs_[i0];

    double sq = 0;
    for (int i = 0; i < n; ++i) sq += frame[i] * frame[i];
    double dBFS = 20.0 * std::log10(std::sqrt(sq / n) + std::numeric_limits<double>::epsilon());

    double semitoneDiff = hasPrev_ ? 12.0 * std::fabs(std::log2(f0 / prevF0_)) : 0.0;
    double conf = strengths_[idxMax] / 2.56;
    bool unreliable = (dBFS < minDb_) || (conf < conf_) || (semitoneDiff > pDelta_);
    if (unreliable) {
        hasPrev_ = false;
        return kNaN;
    }
    prevF0_ = f0;
    hasPrev_ = true;
    return f0;
}

void PitchWidget::onData(const Chunk&) {
    const int step = (int)std::floor(fftSize_ * (1.0 - overlap_));
    int guard = 0;
    while (nextIn_ + fftSize_ <= ctx_.ring.offset() && guard++ < 64) {
        frame_.resize(fftSize_);
        ctx_.ring.copy(0, nextIn_ + fftSize_, fftSize_, frame_.data());
        estimates_.push_back(estimate(frame_));
        nextIn_ += step;
    }
    if (nextIn_ + fftSize_ <= ctx_.ring.offset()) nextIn_ = ctx_.ring.offset() - fftSize_;  // skipped ahead
    const size_t cap = (size_t)(60.0 * kSampleRate / step) + 4;
    while (estimates_.size() > cap) estimates_.pop_front();
}

void PitchWidget::draw(ImVec2 size) {
    // Big readout of the latest estimate.
    double latest = estimates_.empty() ? kNaN : estimates_.back();
    char big[64], note[32];
    if (std::isnan(latest)) {
        std::snprintf(big, sizeof big, "--");
        std::snprintf(note, sizeof note, "--");
    } else if (latest < 1000.0) {
        std::snprintf(big, sizeof big, "%.0f Hz", latest);
        std::snprintf(note, sizeof note, "%s", frequencyToNote(latest).c_str());
    } else {
        std::snprintf(big, sizeof big, "%.2f kHz", latest / 1000.0);
        std::snprintf(note, sizeof note, "%s", frequencyToNote(latest).c_str());
    }
    ImGui::PushFont(nullptr, ImGui::GetFontSize() * 2.0f);
    ImGui::TextColored(rgb(palette::kTeal), "%s", big);
    ImGui::SameLine();
    ImGui::TextColored(rgb(palette::kAmber), "  %s", note);
    float headerH = ImGui::GetItemRectSize().y;
    ImGui::PopFont();

    ImVec2 psize(size.x, std::max(40.f, size.y - headerH - ImGui::GetStyle().ItemSpacing.y));
    if (!ImPlot::BeginPlot("##pitch", psize, kPlotFlags)) return;
    ImPlot::SetupAxis(ImAxis_X1, "Time (sec)", ImPlotAxisFlags_Lock);
    ImPlot::SetupAxisLimits(ImAxis_X1, -duration_, 0.0, ImPlotCond_Always);
    yaxis_.setup(ImAxis_Y1, FreqScale::OctaveC, minFreq_, maxFreq_, "Frequency (Hz)", true, noteLabel);
    ImPlot::SetupFinish();
    yaxis_.drawMinorGrid();

    const int step = (int)std::floor(fftSize_ * (1.0 - overlap_));
    const int num = (int)std::floor(duration_ / ((double)step / kSampleRate)) + 1;
    xs_.resize(num);
    ys_.resize(num);
    const int have = (int)estimates_.size();
    for (int i = 0; i < num; ++i) {
        xs_[i] = -duration_ + (double)duration_ * i / (double)(num - 1);
        int src = have - num + i;  // newest last
        ys_[i] = (src >= 0 && src < have) ? estimates_[src] : kNaN;
    }
    ImPlotSpec s;
    s.LineColor = rgb(palette::kTeal);
    s.LineWeight = 2.0f;
    s.Marker = ImPlotMarker_Circle;
    s.MarkerSize = 2.0f;
    s.MarkerFillColor = rgb(palette::kTeal);
    s.MarkerLineColor = rgb(palette::kTeal);
    s.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
    ImPlot::PlotLine("##pitchline", xs_.data(), ys_.data(), num, s);

    ImPlotPoint mp;
    if (plotHovered(&mp)) {
        ImGui::BeginTooltip();
        ImGui::Text("%.1f sec", mp.x);
        ImGui::Text("%s", formatFrequency(mp.y).c_str());
        ImGui::EndTooltip();
    }
    ImPlot::EndPlot();
}

void PitchWidget::drawSettings() {
    bool re = false;
    re |= ui::spinInt("Min", minFreq_, 10, kSampleRate / 2, " Hz", 5.f);
    re |= ui::spinInt("Max", maxFreq_, 10, kSampleRate / 2, " Hz", 5.f);
    ui::spinInt("Duration", duration_, 5, 60, " s");
    ui::spinDouble("Conf. cut", conf_, 0.0, 1.0, "%.2f", 0.01f);
    double db = minDb_;
    if (ui::spinDouble("Min amplitude", db, -100.0, 0.0, "%.0f dB", 0.5f)) minDb_ = db;
    if (re) {
        if (maxFreq_ <= minFreq_) maxFreq_ = minFreq_ + 10;
        initSwipe();
        hasPrev_ = false;
    }
}

void PitchWidget::save(Settings& s, const std::string& p) const {
    s.set(p + "min", minFreq_);
    s.set(p + "max", maxFreq_);
    s.set(p + "duration", duration_);
    s.set(p + "conf", conf_);
    s.set(p + "mindb", minDb_);
}

void PitchWidget::load(const Settings& s, const std::string& p) {
    minFreq_ = s.getInt(p + "min", minFreq_);
    maxFreq_ = s.getInt(p + "max", maxFreq_);
    duration_ = std::clamp(s.getInt(p + "duration", duration_), 5, 60);
    conf_ = s.getDouble(p + "conf", conf_);
    minDb_ = s.getDouble(p + "mindb", minDb_);
    if (maxFreq_ <= minFreq_) maxFreq_ = minFreq_ + 10;
    initSwipe();
}

}  // namespace fr
