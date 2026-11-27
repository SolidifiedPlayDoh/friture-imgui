#include "widgets/longlevels.hpp"

#include <algorithm>
#include <cmath>

#include "config.hpp"
#include "ui/controls.hpp"
#include "ui/plot_util.hpp"

namespace fr {

namespace {
std::vector<double> gauss(int n, double sigma) {
    std::vector<double> g;
    for (int x = -(n / 2); x <= n / 2; ++x)
        g.push_back(1.0 / (sigma * std::sqrt(2.0 * kPi)) * std::exp(-(double)x * x / (2.0 * sigma * sigma)));
    return g;
}
}  // namespace

LongLevelsWidget::LongLevelsWidget(Context& ctx) : Widget(ctx) {
    setResponse();
    oldIndex_ = ctx_.ring.offset();
}

void LongLevelsWidget::onStreamReset() { oldIndex_ = ctx_.ring.offset(); }

void LongLevelsWidget::setResponse() {
    ndec_ = (int)std::max(0.0, std::floor(std::log2(responseSec_ * (double)kSampleRate / 100.0)));
    subRate_ = (double)kSampleRate / (double)(1 << ndec_);

    // Gaussian FIR final smoother (41 taps, sigma 8) and gaussian decimation stages (11 taps,
    // sigma 2): keeps the squared signal non-negative, unlike IIR ringing.
    std::vector<double> b = gauss(41, 8.0), a(b.size(), 0.0);
    a[0] = 1.0;
    gauss_ = LFilter(b.data(), a.data(), b.size());
    std::vector<double> bs = gauss(11, 2.0), as(bs.size(), 0.0);
    as[0] = 1.0;
    stages_.clear();
    for (int i = 0; i < ndec_; ++i) stages_.push_back({LFilter(bs.data(), as.data(), bs.size()), {}});

    levels_.configure(1, (size_t)std::max(16, lengthSamples_ * 2));
    setDuration();
    ctx_.ring.growIfNeeded((size_t)(1 << ndec_) * 4);
    oldIndex_ = ctx_.ring.offset();
}

void LongLevelsWidget::setDuration() {
    lengthSamples_ = std::max(2, (int)(maxTime_ * subRate_));
    levels_.growIfNeeded((size_t)lengthSamples_ * 2);
}

void LongLevelsWidget::pushSubsampled(const std::vector<double>& x, std::vector<double>& out) {
    out = x;
    for (auto& st : stages_) {
        st.tmp.resize(out.size());
        st.f.process(out.data(), out.size(), st.tmp.data());
        std::vector<double> half((out.size() + 1) / 2);
        for (size_t i = 0; i < half.size(); ++i) half[i] = st.tmp[2 * i];
        out.swap(half);
    }
}

void LongLevelsWidget::onData(const Chunk&) {
    const int64_t index = ctx_.ring.offset();
    int64_t available = index - oldIndex_;
    if (available < 0) {
        available = 0;
        oldIndex_ = index;
    }
    const int needed = 1 << ndec_;
    int64_t realizable = available / needed;
    if (realizable > 4096) {
        oldIndex_ += (realizable - 4096) * needed;
        realizable = 4096;
    }
    in_.resize(needed);
    for (int64_t i = 0; i < realizable; ++i) {
        ctx_.ring.copy(0, oldIndex_, needed, in_.data());
        for (double& v : in_) v *= v;
        pushSubsampled(in_, a_);
        b_.resize(a_.size());
        gauss_.process(a_.data(), a_.size(), b_.data());
        double level = b_.empty() ? 0.0 : b_.back();
        float rms = (float)(10.0 * std::log10(std::max(level, 1e-150)));
        const float* planar[1] = {&rms};
        levels_.push(planar, 1);
        oldIndex_ += needed;
    }
}

void LongLevelsWidget::draw(ImVec2 size) {
    if (!ImPlot::BeginPlot("##longlevels", size, kPlotFlags)) return;
    ImPlot::SetupAxis(ImAxis_X1, "Time (sec)", ImPlotAxisFlags_Lock);
    ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, maxTime_, ImPlotCond_Always);
    setupValueAxis(ImAxis_Y1, levelMin_, levelMax_, "Level (dB FS RMS)");
    ImPlot::SetupFinish();

    const int64_t have = std::min<int64_t>(levels_.offset(), lengthSamples_);
    if (have > 1) {
        const int n = (int)have;
        std::vector<double> v(n);
        levels_.copyLatest(0, (size_t)n, v.data());
        // x: the newest sample sits at the right edge (time = maxTime)
        const int step = std::max(1, n / std::max(64, (int)size.x * 2));
        xs_.clear();
        ys_.clear();
        for (int i = 0; i < n; i += step) {
            xs_.push_back(maxTime_ - (double)(n - 1 - i) / subRate_);
            ys_.push_back(std::clamp(v[i], (double)levelMin_ - 5, (double)levelMax_ + 5));
        }
        ImPlotSpec s;
        s.LineColor = rgb(palette::kTeal);
        s.LineWeight = 1.5f;
        s.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
        ImPlot::PlotLine("##level", xs_.data(), ys_.data(), (int)xs_.size(), s);
    }
    ImPlotPoint mp;
    if (plotHovered(&mp)) {
        ImGui::BeginTooltip();
        ImGui::Text("%.3g sec", mp.x);
        ImGui::Text("%.3g dB", mp.y);
        ImGui::EndTooltip();
    }
    ImPlot::EndPlot();
}

void LongLevelsWidget::drawSettings() {
    ui::spinInt("Max", levelMax_, -200, 200, " dB");
    ui::spinInt("Min", levelMin_, -200, 200, " dB");
    if (ui::spinInt("Response time", responseSec_, 1, 20, " sec")) setResponse();
    if (ui::spinInt("Time span", maxTime_, 5, 3600, " sec")) setDuration();
}

void LongLevelsWidget::save(Settings& s, const std::string& p) const {
    s.set(p + "min", levelMin_);
    s.set(p + "max", levelMax_);
    s.set(p + "response", responseSec_);
    s.set(p + "maxtime", maxTime_);
}

void LongLevelsWidget::load(const Settings& s, const std::string& p) {
    levelMin_ = s.getInt(p + "min", levelMin_);
    levelMax_ = s.getInt(p + "max", levelMax_);
    maxTime_ = std::clamp(s.getInt(p + "maxtime", maxTime_), 5, 3600);
    responseSec_ = std::clamp(s.getInt(p + "response", responseSec_), 1, 20);
    setResponse();
}

}  // namespace fr
