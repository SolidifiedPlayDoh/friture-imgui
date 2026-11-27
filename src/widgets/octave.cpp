#include "widgets/octave.hpp"

#include <algorithm>
#include <cmath>
#include <string>

#include "config.hpp"
#include "dsp/scales.hpp"
#include "ui/controls.hpp"
#include "ui/plot_util.hpp"

namespace fr {

namespace {
constexpr int kBands[] = {1, 3, 6, 12, 24};
const char* const kBandNames[] = {"1", "3", "6", "12", "24"};
const char* const kWeightNames[] = {"None", "A", "B", "C"};
const char* const kResponseNames[] = {"25 ms (Impulse)", "125 ms (Fast)", "300 ms", "1s (Slow)",
                                      "5s (Very Slow)"};
constexpr double kResponseSecs[] = {0.025, 0.125, 0.3, 1.0, 5.0};
}  // namespace

OctaveWidget::OctaveWidget(Context& ctx) : Widget(ctx) { setBands(bandsIndex_); }

void OctaveWidget::setBands(int index) {
    bandsIndex_ = std::clamp(index, 0, 4);
    bank_.setBandsPerOctave(kBands[bandsIndex_]);
    sp_.assign(bank_.bandCount(), 0.0);
    updateResponse();
}

void OctaveWidget::updateResponse() {
    const double w = 0.65;
    const double resp = kResponseSecs[std::clamp(responseIndex_, 0, 4)];
    alphas_.resize(bank_.bandCount());
    for (int k = 0; k < bank_.bandCount(); ++k) {
        double n = resp * kSampleRate / bank_.decimationOf(k);
        alphas_[k] = 1.0 - std::pow(1.0 - w, 1.0 / (n + 1.0));
    }
}

void OctaveWidget::onData(const Chunk& chunk) {
    if (chunk.frames() == 0) return;
    bank_.process(chunk.ch[0].data(), chunk.frames(), y_);
    const int count = bank_.bandCount();
    for (int k = 0; k < count; ++k) {
        double v = sp_[k];
        const double a = alphas_[k];
        for (double x : y_[k]) v = a * (x * x) + (1.0 - a) * v;
        sp_[k] = v;
    }
    const std::vector<double>* w = nullptr;
    if (weighting_ == 1) w = &bank_.weightA();
    else if (weighting_ == 2) w = &bank_.weightB();
    else if (weighting_ == 3) w = &bank_.weightC();
    db_.resize(count);
    for (int k = 0; k < count; ++k) db_[k] = 10.0 * std::log10(sp_[k] + 1e-30) + (w ? (*w)[k] : 0.0);
}

void OctaveWidget::draw(ImVec2 size) {
    const std::vector<double>& flow = bank_.flow();
    const std::vector<double>& fhigh = bank_.fhigh();
    const std::vector<double>& fi = bank_.fi();
    if (!ImPlot::BeginPlot("##octave", size, kPlotFlags)) return;

    const double lo = flow.front(), hi = fhigh.back();
    ImPlot::SetupAxis(ImAxis_X1, "Frequency band (Hz)", ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines);
    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Log10);
    ImPlot::SetupAxisLimits(ImAxis_X1, lo, hi, ImPlotCond_Always);

    // Label the bands, thinning them out so labels never overlap.
    std::vector<double> tickPos;
    std::vector<std::string> tickText;
    {
        const double px = std::max(1.0, (double)ImGui::GetContentRegionAvail().x);
        const double span = std::log(hi / lo);
        const double perBandPx = px * std::log(fhigh[0] / flow[0]) / span;
        const double minLabelPx = ImGui::GetFontSize() * 3.2;
        int stride = std::max(1, (int)std::ceil(minLabelPx / std::max(perBandPx, 1.0)));
        // anchor labels so that the 1 kHz band is always labelled
        int anchor = 0;
        for (size_t i = 0; i < fi.size(); ++i) if (fi[i] == 1000.0) anchor = (int)i;
        for (int i = 0; i < (int)fi.size(); ++i)
            if (((i - anchor) % stride + stride) % stride == 0) {
                tickPos.push_back(fi[i]);
                tickText.push_back(bank_.nominal()[i]);
            }
    }
    std::vector<const char*> tickPtrs;
    for (auto& s : tickText) tickPtrs.push_back(s.c_str());
    ImPlot::SetupAxisTicks(ImAxis_X1, tickPos.data(), (int)tickPos.size(), tickPtrs.data(), false);

    const char* ylabel = weighting_ == 0 ? "Level (dB)" : (weighting_ == 1 ? "Level (dB A)" : (weighting_ == 2 ? "Level (dB B)" : "Level (dB C)"));
    setupValueAxis(ImAxis_Y1, specMin_, specMax_, ylabel);
    ImPlot::SetupFinish();

    if (db_.size() == fi.size()) {
        ImDrawList* dl = ImPlot::GetPlotDrawList();
        ImPlot::PushPlotClipRect();
        const double range = std::max(1.0, (double)(specMax_ - specMin_));
        const double gap = 0.12;  // fraction of a band left empty between bars
        for (size_t k = 0; k < fi.size(); ++k) {
            double z = std::clamp((db_[k] - specMin_) / range, 0.0, 1.0);
            double ctr = std::sqrt(flow[k] * fhigh[k]);
            double ratio = std::pow(fhigh[k] / flow[k], 0.5 * (1.0 - gap));
            double x0 = ctr / ratio, x1 = ctr * ratio;
            double ytop = std::clamp(db_[k], (double)specMin_, (double)specMax_);
            ImVec2 p0 = ImPlot::PlotToPixels(x0, ytop);
            ImVec2 p1 = ImPlot::PlotToPixels(x1, (double)specMin_);
            ImU32 c = IM_COL32((int)((0.10 + 0.10 * z) * 255), (int)((0.35 + 0.65 * z) * 255),
                               (int)((0.80 + 0.20 * z) * 255), 255);
            if (p1.y > p0.y) dl->AddRectFilled(p0, p1, c);
        }
        ImPlot::PopPlotClipRect();

        ImPlotPoint mp;
        if (plotHovered(&mp) && mp.x > 0) {
            for (size_t k = 0; k < fi.size(); ++k)
                if (mp.x >= flow[k] && mp.x < fhigh[k]) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s Hz", bank_.nominal()[k].c_str());
                    ImGui::Text("%.1f dB", db_[k]);
                    ImGui::EndTooltip();
                    break;
                }
        }
    }
    ImPlot::EndPlot();
}

void OctaveWidget::drawSettings() {
    int bi = bandsIndex_;
    if (ui::combo("Bands per octave", bi, kBandNames, 5)) setBands(bi);
    ui::spinInt("Min", specMin_, -200, 200, " dB");
    ui::spinInt("Max", specMax_, -200, 200, " dB");
    ui::combo("Middle-ear weighting", weighting_, kWeightNames, 4);
    if (ui::combo("Response time", responseIndex_, kResponseNames, 5)) updateResponse();
}

void OctaveWidget::save(Settings& s, const std::string& p) const {
    s.set(p + "bands", bandsIndex_);
    s.set(p + "specmin", specMin_);
    s.set(p + "specmax", specMax_);
    s.set(p + "weighting", weighting_);
    s.set(p + "response", responseIndex_);
}

void OctaveWidget::load(const Settings& s, const std::string& p) {
    specMin_ = s.getInt(p + "specmin", specMin_);
    specMax_ = s.getInt(p + "specmax", specMax_);
    weighting_ = std::clamp(s.getInt(p + "weighting", weighting_), 0, 3);
    responseIndex_ = std::clamp(s.getInt(p + "response", responseIndex_), 0, 4);
    setBands(s.getInt(p + "bands", bandsIndex_));
}

}  // namespace fr
