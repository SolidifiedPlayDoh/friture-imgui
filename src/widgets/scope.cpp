#include "widgets/scope.hpp"

#include <algorithm>
#include <cmath>

#include "config.hpp"
#include "ui/controls.hpp"
#include "ui/plot_util.hpp"

namespace fr {

void ScopeWidget::onData(const Chunk&) {
    const int width = std::max(8, (int)(timeRange_ * 1e-3 * kSampleRate));
    ctx_.ring.growIfNeeded((size_t)width * 4);
    const int nch = ctx_.ring.channels();
    const size_t n = (size_t)width * 2;
    if (ctx_.ring.offset() < (int64_t)n) return;

    buf0_.resize(n);
    ctx_.ring.copyLatest(0, n, buf0_.data());
    if (nch > 1) {
        buf1_.resize(n);
        ctx_.ring.copyLatest(1, n, buf1_.data());
    }
    two_ = nch > 1;

    // Trigger on the first channel, searching the middle half of the data.
    const int searchStart = width / 2;
    const int searchStop = (int)n - width / 2;
    double mx = -1e300;
    for (int i = searchStart; i < searchStop; ++i) mx = std::max(mx, buf0_[i]);
    const double level = mx * 2.0 / 3.0;
    int pos = -1;
    for (int i = searchStart; i + 1 < searchStop; ++i)
        if (buf0_[i] < level && buf0_[i + 1] >= level) {
            pos = i;
            break;
        }
    if (pos < 0) return;

    const int half = width / 2;
    const int begin = pos - half;
    const int end = pos + half;
    if (begin < 0 || end > (int)n) return;
    const int len = end - begin;
    y1_.assign(buf0_.begin() + begin, buf0_.begin() + end);
    if (two_) y2_.assign(buf1_.begin() + begin, buf1_.begin() + end);
    else y2_.clear();
    t_.resize(len);
    for (int i = 0; i < len; ++i) t_[i] = (double)(i - half) / kSampleRate * 1e3;
}

namespace {
// Keep min and max of each pixel column so peaks survive on dense waveforms.
void minMaxDecimate(const std::vector<double>& t, const std::vector<double>& y, int cols,
                    std::vector<double>& ox, std::vector<double>& oy) {
    ox.clear();
    oy.clear();
    const size_t n = y.size();
    if (n < (size_t)cols * 4) {
        ox = t;
        oy = y;
        return;
    }
    const double per = (double)n / cols;
    for (int c = 0; c < cols; ++c) {
        size_t a = (size_t)(c * per), b = std::min(n, (size_t)((c + 1) * per));
        if (b <= a) continue;
        size_t imin = a, imax = a;
        for (size_t i = a; i < b; ++i) {
            if (y[i] < y[imin]) imin = i;
            if (y[i] > y[imax]) imax = i;
        }
        size_t first = std::min(imin, imax), second = std::max(imin, imax);
        ox.push_back(t[first]); oy.push_back(y[first]);
        if (second != first) { ox.push_back(t[second]); oy.push_back(y[second]); }
    }
}
}  // namespace

void ScopeWidget::draw(ImVec2 size) {
    if (!ImPlot::BeginPlot("##scope", size, kPlotFlags)) return;
    ImPlot::SetupAxis(ImAxis_X1, "Time (ms)", ImPlotAxisFlags_Lock);
    ImPlot::SetupAxisLimits(ImAxis_X1, -timeRange_ / 2, timeRange_ / 2, ImPlotCond_Always);
    setupValueAxis(ImAxis_Y1, -1.0, 1.0, "Signal");
    ImPlot::SetupFinish();

    if (!t_.empty()) {
        const int cols = std::max(64, (int)size.x);
        ImPlotSpec s1;
        s1.LineColor = rgb(palette::kTeal);
        s1.LineWeight = 1.5f;
        s1.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
        minMaxDecimate(t_, y1_, cols, dx_, dy_);
        ImPlot::PlotLine("##ch1", dx_.data(), dy_.data(), (int)dx_.size(), s1);
        if (two_ && y2_.size() == t_.size()) {
            ImPlotSpec s2 = s1;
            s2.LineColor = rgb(palette::kBlue);
            minMaxDecimate(t_, y2_, cols, dx_, dy2_);
            ImPlot::PlotLine("##ch2", dx_.data(), dy2_.data(), (int)dx_.size(), s2);
        }
    }

    ImPlotPoint mp;
    if (plotHovered(&mp)) {
        ImGui::BeginTooltip();
        ImGui::Text("%.3g ms", mp.x);
        ImGui::Text("%.3g", mp.y);
        ImGui::EndTooltip();
    }
    ImPlot::EndPlot();
}

void ScopeWidget::drawSettings() {
    ui::spinDouble("Time range", timeRange_, 0.1, 1000.0, "%.1f ms", 0.5f);
}

void ScopeWidget::save(Settings& s, const std::string& p) const { s.set(p + "timerange", timeRange_); }
void ScopeWidget::load(const Settings& s, const std::string& p) {
    timeRange_ = s.getDouble(p + "timerange", timeRange_);
}

}  // namespace fr
