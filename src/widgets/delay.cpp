#include "widgets/delay.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

#include "config.hpp"
#include "dsp/fft.hpp"
#include "ui/controls.hpp"

namespace fr {

DelayWidget::DelayWidget(Context& ctx) : Widget(ctx) { setup(); }

void DelayWidget::setup() {
    subRate_ = (double)kSampleRate / (double)(1 << kNdec);
    size_t length = (size_t)(2.0 * delayRange_ * subRate_) & ~(size_t)1;  // even for irfft
    r0_.configure(1, std::max<size_t>(16, length * 4));
    r1_.configure(1, std::max<size_t>(16, length * 4));
    oldIndex_ = 0;
    oldX_.clear();
}

namespace {
double stddev(const std::vector<double>& v) {
    double m = 0;
    for (double x : v) m += x;
    m /= (double)v.size();
    double s = 0;
    for (double x : v) s += (x - m) * (x - m);
    return std::sqrt(s / (double)v.size());
}
}  // namespace

void DelayWidget::onData(const Chunk& chunk) {
    if (chunk.channels < 2) {
        two_ = false;
        return;
    }
    two_ = true;
    if (chunk.frames() == 0) return;

    // Decimate both channels by 4 and push to the 1-channel history rings.
    tmpA_.assign(chunk.ch[0].begin(), chunk.ch[0].end());
    tmpB_.assign(chunk.ch[1].begin(), chunk.ch[1].end());
    std::vector<double> nxt;
    for (int i = 0; i < kNdec; ++i) {
        dec0_[i].process(tmpA_.data(), tmpA_.size(), nxt);
        tmpA_.swap(nxt);
        dec1_[i].process(tmpB_.data(), tmpB_.size(), nxt);
        tmpB_.swap(nxt);
    }
    std::vector<float> f0(tmpA_.begin(), tmpA_.end()), f1(tmpB_.begin(), tmpB_.end());
    const float* p0[1] = {f0.data()};
    const float* p1[1] = {f1.data()};
    r0_.push(p0, f0.size());
    r1_.push(p1, f1.size());

    const int64_t index = r0_.offset();
    int64_t available = index - oldIndex_;
    if (available < 0) {
        available = 0;
        oldIndex_ = index;
    }
    const double time = 2.0 * delayRange_;
    const size_t length = (size_t)(time * subRate_) & ~(size_t)1;
    const int64_t needed = std::max<int64_t>(1, (int64_t)(0.5 * (double)length));
    int64_t realizable = available / needed;
    if (realizable > 8) {
        oldIndex_ += (realizable - 8) * needed;
        realizable = 8;
    }
    for (int64_t i = 0; i < realizable; ++i) {
        oldIndex_ += needed;
        estimate(oldIndex_);
    }
}

void DelayWidget::estimate(int64_t idx) {
    const double time = 2.0 * delayRange_;
    const size_t n = (size_t)(time * subRate_) & ~(size_t)1;
    if (n < 8) return;
    d0_.resize(n);
    d1_.resize(n);
    r0_.copy(0, idx, n, d0_.data());
    r1_.copy(0, idx, n, d1_.data());

    const double std0 = stddev(d0_), std1 = stddev(d1_);
    double xcorrMaxNorm = 0.0;
    if (std0 > 0.0 && std1 > 0.0) {
        // Generalized cross-correlation with PHAT weighting.
        double m0 = 0, m1 = 0;
        for (size_t i = 0; i < n; ++i) { m0 += d0_[i]; m1 += d1_[i]; }
        m0 /= (double)n; m1 /= (double)n;
        std::vector<double> a(n), b(n);
        for (size_t i = 0; i < n; ++i) {
            double w = 0.5 - 0.5 * std::cos(2.0 * kPi * (double)i / (double)(n - 1));  // numpy.hanning
            a[i] = (d0_[i] - m0) * w;
            b[i] = (d1_[i] - m1) * w;
        }
        const size_t bins = n / 2 + 1;
        std::vector<std::complex<double>> A(bins), B(bins), G(bins);
        rfft(a.data(), n, A.data());
        rfft(b.data(), n, B.data());
        double mx = 0;
        for (size_t i = 0; i < bins; ++i) {
            G[i] = std::conj(A[i]) * B[i];
            mx = std::max(mx, std::abs(G[i]));
        }
        for (size_t i = 0; i < bins; ++i) G[i] *= 1.0 / (1e-10 * mx + std::abs(G[i]));
        std::vector<double> xc(n);
        irfft(G.data(), n, xc.data());

        if (oldX_.size() == xc.size()) {
            const double alpha = 0.3;
            smoothed_.resize(n);
            for (size_t i = 0; i < n; ++i) smoothed_[i] = alpha * xc[i] + (1.0 - alpha) * oldX_[i];
        } else {
            smoothed_ = xc;
        }
        size_t im = 0;
        double best = -1;
        for (size_t i = 0; i < n; ++i)
            if (std::fabs(smoothed_[i]) > best) { best = std::fabs(smoothed_[i]); im = i; }
        extremum_ = smoothed_[im];
        xcorrMaxNorm = std::fabs(smoothed_[im]) / (3.0 * stddev(smoothed_));
        delayMs_ = 1e3 * (double)im / subRate_;
        if (delayMs_ > 1e3 * time / 2.0) delayMs_ -= 1e3 * time;  // large delays are really negative
        oldX_ = smoothed_;
    } else {
        delayMs_ = 0;
        xcorrMaxNorm = 0;
        extremum_ = 0;
    }
    distanceM_ = delayMs_ * 1e-3 * 340.0;  // speed of sound

    const double slope = 0.12;
    const int p = 3;
    double x = (xcorrMaxNorm > 1.0) ? (xcorrMaxNorm - 1.0) : 0.0;
    x = std::pow(slope * x, p);
    correlation_ = (int)((x / (1.0 + x)) * 100.0);
}

void DelayWidget::draw(ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float fs = ImGui::GetFontSize();
    auto center = [&](const char* text, float y, float scale, ImU32 col) {
        ImGui::PushFont(nullptr, fs * scale);
        ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, y), col, text);
        ImGui::PopFont();
        return ts.y;
    };

    if (!two_) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + size.x);
        ImGui::TextDisabled("Delay estimator only works with two channels.\nSelect two-channels mode in the settings window.");
        ImGui::PopTextWrapPos();
        ImGui::Dummy(size);
        return;
    }

    char buf[64];
    float y = p.y + size.y * 0.12f;
    std::snprintf(buf, sizeof buf, "%.1f ms", delayMs_);
    y += center(buf, y, 2.6f, IM_COL32(39, 224, 184, 255)) + fs * 0.4f;
    std::snprintf(buf, sizeof buf, "= %.2f m", distanceM_);
    y += center(buf, y, 1.6f, IM_COL32(230, 230, 230, 255)) + fs * 1.2f;

    // correlation bar
    const float bw = std::min(size.x * 0.7f, fs * 22.f), bh = fs * 1.1f;
    const float bx = p.x + (size.x - bw) * 0.5f;
    dl->AddRectFilled(ImVec2(bx, y), ImVec2(bx + bw, y + bh), IM_COL32(37, 37, 37, 255), 2.f);
    dl->AddRectFilled(ImVec2(bx, y), ImVec2(bx + bw * correlation_ / 100.f, y + bh), IM_COL32(42, 125, 225, 255), 2.f);
    y += bh + fs * 0.4f;
    std::snprintf(buf, sizeof buf, "Correlation: %d%%", correlation_);
    y += center(buf, y, 1.0f, IM_COL32(154, 154, 154, 255)) + fs * 0.6f;
    center(extremum_ >= 0 ? "In-phase" : "Reversed phase", y, 1.2f, IM_COL32(240, 180, 41, 255));
    ImGui::Dummy(size);
}

void DelayWidget::drawSettings() {
    if (ui::spinDouble("Delay range (s)", delayRange_, 0.1, 1000.0, "%.1f s", 0.1f)) setup();
    ImGui::TextDisabled("Maximum delay that is reliably estimated.");
}

void DelayWidget::save(Settings& s, const std::string& p) const { s.set(p + "range", delayRange_); }
void DelayWidget::load(const Settings& s, const std::string& p) {
    delayRange_ = s.getDouble(p + "range", delayRange_);
    setup();
}

}  // namespace fr
