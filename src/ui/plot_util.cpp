#include "ui/plot_util.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fr {

namespace {

double lowerBound(FreqScale s) {
    switch (s) {
        case FreqScale::Linear: return -1e12;
        case FreqScale::Mel: return -699.9;
        case FreqScale::Erb: return -228.0;
        default: return 1e-20;
    }
}

double fwd(double v, void* user) {
    FreqScale s = (FreqScale)(intptr_t)user;
    return scaleTransform(s, std::max(v, lowerBound(s)));
}

double inv(double v, void* user) {
    FreqScale s = (FreqScale)(intptr_t)user;
    return scaleInverse(s, v);
}

}  // namespace

void FreqAxis::setup(ImAxis axis, FreqScale scale, double lo, double hi, const char* label,
                     bool tickLabels, LabelFn fmt) {
    if (hi < lo) std::swap(lo, hi);
    if (hi <= lo) hi = lo + 1.0;
    ImPlotAxisFlags flags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines * 0;
    if (!tickLabels) flags |= ImPlotAxisFlags_NoTickLabels;
    ImPlot::SetupAxis(axis, label, flags);
    if (scale != FreqScale::Linear)
        ImPlot::SetupAxisScale(axis, fwd, inv, (void*)(intptr_t)scale);
    ImPlot::SetupAxisLimits(axis, lo, hi, ImPlotCond_Always);

    scaleTicks(scale, lo, hi, major_, minor_);
    if (!major_.empty()) {
        std::vector<std::string> labels;
        std::vector<const char*> ptrs;
        labels.reserve(major_.size());
        for (double f : major_) labels.push_back(fmt ? fmt(f) : formatAxisFreq(f));
        for (auto& s : labels) ptrs.push_back(s.c_str());
        ImPlot::SetupAxisTicks(axis, major_.data(), (int)major_.size(), ptrs.data(), false);
    } else {
        // Few/no major ticks in view: let ImPlot place its own.
        minor_.clear();
    }
}

void FreqAxis::drawMinorGrid() {
    if (minor_.empty()) return;
    ImPlotSpec spec;
    spec.LineColor = rgb(0x2a2a2a, 1.f);
    spec.LineWeight = 1.f;
    spec.Flags = ImPlotItemFlags_NoLegend | ImPlotItemFlags_NoFit;
    ImPlot::PlotInfLines("##minor", minor_.data(), (int)minor_.size(), spec);
}

void setupValueAxis(ImAxis axis, double lo, double hi, const char* label, bool tickLabels) {
    ImPlotAxisFlags flags = ImPlotAxisFlags_Lock;
    if (!tickLabels) flags |= ImPlotAxisFlags_NoTickLabels;
    ImPlot::SetupAxis(axis, label, flags);
    ImPlot::SetupAxisLimits(axis, lo, hi, ImPlotCond_Always);
}

bool plotHovered(ImPlotPoint* p) {
    if (!ImPlot::IsPlotHovered()) return false;
    if (p) *p = ImPlot::GetPlotMousePos();
    return true;
}

}  // namespace fr

namespace fr {

void decimateIndices(const double* x, const double* y, size_t i0, size_t i1, FreqScale scale,
                     double lo, double hi, int cols, std::vector<int>& idx) {
    idx.clear();
    if (i1 <= i0) return;
    cols = std::max(cols, 16);
    const double tlo = scaleTransform(scale, lo), thi = scaleTransform(scale, hi);
    const double span = thi - tlo;
    if (!(span > 0)) {
        for (size_t i = i0; i < i1; ++i) idx.push_back((int)i);
        return;
    }
    long lastCol = -1000000;
    for (size_t i = i0; i < i1; ++i) {
        double t = scaleTransform(scale, x[i]);
        long col = (long)std::floor((t - tlo) / span * cols);
        if (!idx.empty() && col == lastCol) {
            if (y[i] > y[idx.back()]) idx.back() = (int)i;
        } else {
            idx.push_back((int)i);
            lastCol = col;
        }
    }
}

}  // namespace fr
