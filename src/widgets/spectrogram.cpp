#include "widgets/spectrogram.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config.hpp"
#include "dsp/generated_cmap.hpp"
#include "ui/controls.hpp"
#include "ui/gl_include.hpp"

namespace fr {

namespace {
constexpr int kFftSizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
const char* const kFftNames[] = {"32 points",   "64 points",   "128 points", "256 points",
                                 "512 points",  "1024 points", "2048 points", "4096 points",
                                 "8192 points", "16384 points"};
const char* const kWeightNames[] = {"None", "A", "B", "C"};
const char* const kScaleNames[] = {"Linear", "Logarithmic", "Mel", "ERB", "Octave"};
constexpr int kMaxTexW = 4096;

uint32_t colormap(double v) {
    v = std::clamp(v, 0.0, 1.0);
    int i = (int)std::lround(v * (tables::kCmapSize - 1));
    return IM_COL32((int)(tables::kCmap[i][0] * 255), (int)(tables::kCmap[i][1] * 255),
                    (int)(tables::kCmap[i][2] * 255), 255);
}
}  // namespace

SpectrogramWidget::SpectrogramWidget(Context& ctx) : Widget(ctx) { rebuild(); }

SpectrogramWidget::~SpectrogramWidget() {
    if (tex_) {
        GLuint t = tex_;
        glDeleteTextures(1, &t);
    }
}

void SpectrogramWidget::onStreamReset() { oldIndex_ = ctx_.ring.offset(); }

void SpectrogramWidget::rebuild() {
    fftSize_ = kFftSizes[std::clamp(fftIndex_, 0, 9)];
    proc_.setFftSize(fftSize_);
    w_ = proc_.weighting(weighting_);
    ctx_.ring.growIfNeeded((size_t)fftSize_ * 4);
    oldIndex_ = ctx_.ring.offset();

    // Frequency of every texture row, top (max) to bottom (min), uniform on the scale.
    const double lo = std::min(minFreq_, maxFreq_), hi = std::max(minFreq_, maxFreq_);
    const double tlo = scaleTransform(scale_, lo), thi = scaleTransform(scale_, std::max(hi, lo + 1));
    rowFreq_.resize(kTexH);
    for (int j = 0; j < kTexH; ++j)
        rowFreq_[j] = scaleInverse(scale_, thi + (tlo - thi) * j / (double)(kTexH - 1));

    rebuildTexture();
}

void SpectrogramWidget::rebuildTexture() {
    const double colRate = kSampleRate / (fftSize_ * (1.0 - overlap_));
    mergeK_ = std::max(1, (int)std::ceil(colRate * timeRange_ / kMaxTexW));
    texW_ = std::clamp((int)std::ceil(timeRange_ * colRate / mergeK_), 2, kMaxTexW);
    head_ = 0;
    accCount_ = 0;
    acc_.assign(kTexH, 0.0);
    column_.assign(kTexH, colormap(0.0));

    if (tex_) {
        GLuint t = tex_;
        glDeleteTextures(1, &t);
        tex_ = 0;
    }
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    std::vector<uint32_t> blank((size_t)texW_ * kTexH, colormap(0.0));
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texW_, kTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, blank.data());
    tex_ = t;
}

void SpectrogramWidget::uploadColumn(const std::vector<double>& rows) {
    for (int j = 0; j < kTexH; ++j) column_[j] = colormap(rows[j]);
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, head_, 0, 1, kTexH, GL_RGBA, GL_UNSIGNED_BYTE, column_.data());
    head_ = (head_ + 1) % texW_;
}

void SpectrogramWidget::pushColumn(const std::vector<double>& norm) {
    // Resample the linear-frequency column onto the scaled rows.
    const size_t bins = norm.size();
    const double binWidth = (double)(kSampleRate / 2) / (double)(bins - 1);
    rows_.resize(kTexH);
    for (int j = 0; j < kTexH; ++j) {
        double pos = rowFreq_[j] / binWidth;
        if (pos <= 0) { rows_[j] = norm[0]; continue; }
        if (pos >= (double)(bins - 1)) { rows_[j] = norm[bins - 1]; continue; }
        size_t i = (size_t)pos;
        double fr = pos - (double)i;
        rows_[j] = norm[i] * (1.0 - fr) + norm[i + 1] * fr;
    }
    if (mergeK_ == 1) {
        uploadColumn(rows_);
        return;
    }
    for (int j = 0; j < kTexH; ++j) acc_[j] += rows_[j];
    if (++accCount_ >= mergeK_) {
        for (int j = 0; j < kTexH; ++j) acc_[j] /= accCount_;
        uploadColumn(acc_);
        std::fill(acc_.begin(), acc_.end(), 0.0);
        accCount_ = 0;
    }
}

void SpectrogramWidget::onData(const Chunk&) {
    const int64_t index = ctx_.ring.offset();
    int64_t available = index - oldIndex_;
    if (available < 0) {
        available = 0;
        oldIndex_ = index;
    }
    const int needed = (int)(fftSize_ * (1.0 - overlap_));
    int64_t realizable = available / needed;
    if (realizable <= 0) return;
    if (realizable > 256) {
        oldIndex_ += (realizable - 256) * needed;
        realizable = 256;
    }
    in_.resize(fftSize_);
    const double range = std::max(1.0, (double)(specMax_ - specMin_));
    for (int64_t i = 0; i < realizable; ++i) {
        ctx_.ring.copy(0, oldIndex_, fftSize_, in_.data());
        proc_.analyze(in_.data(), sp_);
        norm_.resize(sp_.size());
        for (size_t k = 0; k < sp_.size(); ++k) {
            double dB = 10.0 * std::log10(sp_[k] + 1e-30) + w_[k];
            norm_[k] = (dB - specMin_) / range;
        }
        pushColumn(norm_);
        oldIndex_ += needed;
    }
}

void SpectrogramWidget::draw(ImVec2 size) {
    const float barW = ImGui::GetFontSize() * 3.6f;
    const ImVec2 plotSize(std::max(40.f, size.x - barW), size.y);
    const double lo = std::min(minFreq_, maxFreq_), hi = std::max(minFreq_, maxFreq_);

    if (ImPlot::BeginPlot("##spectrogram", plotSize, kPlotFlags)) {
        ImPlot::SetupAxis(ImAxis_X1, "Time (s)", ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines);
        ImPlot::SetupAxisLimits(ImAxis_X1, -timeRange_, 0.0, ImPlotCond_Always);
        yaxis_.setup(ImAxis_Y1, scale_, lo, hi, "Frequency (Hz)");
        ImPlot::SetupFinish();

        if (tex_) {
            // The texture is a ring (the ImGui backend clamps, so no GL_REPEAT): draw oldest then newest piece.
            const ImTextureRef ref((ImTextureID)(intptr_t)tex_);
            const double top = std::max(hi, lo + 1);
            const double frac = (double)head_ / (double)texW_;  // share of the ring holding the newest data
            const double xSplit = -timeRange_ * frac;
            ImPlot::PlotImage("##imgOld", ref, ImPlotPoint(-timeRange_, lo), ImPlotPoint(xSplit, top),
                              ImVec2((float)frac, 0.f), ImVec2(1.f, 1.f));
            if (head_ > 0)
                ImPlot::PlotImage("##imgNew", ref, ImPlotPoint(xSplit, lo), ImPlotPoint(0.0, top), ImVec2(0.f, 0.f),
                                  ImVec2((float)frac, 1.f));
        }
        yaxis_.drawMinorGrid();

        ImPlotPoint mp;
        if (plotHovered(&mp)) {
            ImGui::BeginTooltip();
            ImGui::Text("%.2f s", mp.x);
            ImGui::Text("%s Hz", formatAxisFreq(mp.y).c_str());
            ImGui::EndTooltip();
        }
        ImPlot::EndPlot();
    }

    // Colour scale to the right of the plot.
    ImGui::SameLine(0, 4);
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float topPad = ImGui::GetStyle().WindowPadding.y + 2.f;
    const float botPad = ImGui::GetFontSize() * 2.2f;
    const float x0 = p.x + 2.f, x1 = x0 + ImGui::GetFontSize() * 0.9f;
    const float y0 = p.y + topPad, y1 = p.y + size.y - botPad;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int segs = 64;
    for (int i = 0; i < segs; ++i) {
        float ya = y0 + (y1 - y0) * i / segs, yb = y0 + (y1 - y0) * (i + 1) / segs;
        ImU32 ca = colormap(1.0 - (double)i / segs), cb = colormap(1.0 - (double)(i + 1) / segs);
        dl->AddRectFilledMultiColor(ImVec2(x0, ya), ImVec2(x1, yb), ca, ca, cb, cb);
    }
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(60, 60, 60, 255));
    const int ticks = 5;
    for (int i = 0; i <= ticks; ++i) {
        float f = (float)i / ticks;
        float y = y0 + (y1 - y0) * f;
        char b[16];
        std::snprintf(b, sizeof b, "%d", (int)std::lround(specMax_ + (specMin_ - specMax_) * f));
        dl->AddLine(ImVec2(x1, y), ImVec2(x1 + 3, y), IM_COL32(120, 120, 120, 255));
        dl->AddText(ImVec2(x1 + 5, y - ImGui::GetFontSize() * 0.5f), IM_COL32(154, 154, 154, 255), b);
    }
    ImGui::Dummy(ImVec2(barW, 1));
}

void SpectrogramWidget::drawSettings() {
    bool rb = false;
    rb |= ui::spinDouble("Time range", timeRange_, 0.1, 1000.0, "%.1f s", 0.1f);
    rb |= ui::combo("FFT size", fftIndex_, kFftNames, 10);
    int sc = (int)scale_;
    if (ui::combo("Frequency scale", sc, kScaleNames, kUserScaleCount)) { scale_ = (FreqScale)sc; rb = true; }
    rb |= ui::spinInt("Min frequency", minFreq_, 20, kSampleRate / 2, " Hz", 10.f);
    rb |= ui::spinInt("Max frequency", maxFreq_, 20, kSampleRate / 2, " Hz", 100.f);
    ui::spinInt("Min", specMin_, -200, 200, " dB");
    ui::spinInt("Max", specMax_, -200, 200, " dB");
    rb |= ui::combo("Middle-ear weighting", weighting_, kWeightNames, 4);
    if (rb) rebuild();
}

void SpectrogramWidget::save(Settings& s, const std::string& p) const {
    s.set(p + "fft", fftIndex_);
    s.set(p + "scale", (int)scale_);
    s.set(p + "minfreq", minFreq_);
    s.set(p + "maxfreq", maxFreq_);
    s.set(p + "specmin", specMin_);
    s.set(p + "specmax", specMax_);
    s.set(p + "timerange", timeRange_);
    s.set(p + "weighting", weighting_);
}

void SpectrogramWidget::load(const Settings& s, const std::string& p) {
    fftIndex_ = std::clamp(s.getInt(p + "fft", fftIndex_), 0, 9);
    scale_ = (FreqScale)std::clamp(s.getInt(p + "scale", (int)scale_), 0, kUserScaleCount - 1);
    minFreq_ = s.getInt(p + "minfreq", minFreq_);
    maxFreq_ = s.getInt(p + "maxfreq", maxFreq_);
    specMin_ = s.getInt(p + "specmin", specMin_);
    specMax_ = s.getInt(p + "specmax", specMax_);
    timeRange_ = s.getDouble(p + "timerange", timeRange_);
    weighting_ = std::clamp(s.getInt(p + "weighting", weighting_), 0, 3);
    rebuild();
}

}  // namespace fr
