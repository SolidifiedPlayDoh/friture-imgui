#pragma once
// ImPlot helpers: non-linear frequency axes with Friture's tick logic, shared styling.

#include <string>
#include <vector>

#include "dsp/scales.hpp"
#include "imgui.h"
#include "implot.h"

namespace fr {

constexpr ImPlotFlags kPlotFlags = ImPlotFlags_NoTitle | ImPlotFlags_NoLegend | ImPlotFlags_NoMenus |
                                   ImPlotFlags_NoBoxSelect | ImPlotFlags_NoInputs |
                                   ImPlotFlags_NoMouseText;

inline ImVec4 rgb(unsigned c, float a = 1.f) {
    return ImVec4(((c >> 16) & 0xff) / 255.f, ((c >> 8) & 0xff) / 255.f, (c & 0xff) / 255.f, a);
}

// Colours shared by the analyzers.
namespace palette {
constexpr unsigned kTeal = 0x27e0b8;
constexpr unsigned kBlue = 0x5aa9ff;
constexpr unsigned kAmber = 0xf0b429;
}  // namespace palette

class FreqAxis {
public:
    using LabelFn = std::string (*)(double);
    // Call between BeginPlot() and the first plot item. `fmt` customises tick labels.
    void setup(ImAxis axis, FreqScale scale, double lo, double hi, const char* label,
               bool tickLabels = true, LabelFn fmt = nullptr);
    const std::vector<double>& major() const { return major_; }
    // Faint minor grid lines (call after the axis is set up, before items).
    void drawMinorGrid();

private:
    std::vector<double> major_, minor_;
};

// Fixed-limits value axis (e.g. dB).
void setupValueAxis(ImAxis axis, double lo, double hi, const char* label,
                    bool tickLabels = true);

// Pick which bins of a spectrum to draw: at most ~`cols` columns on the (possibly
// non-linear) x axis, keeping the maximum bin of each column. Bins [i0, i1) are considered.
void decimateIndices(const double* x, const double* y, size_t i0, size_t i1, FreqScale scale,
                     double lo, double hi, int cols, std::vector<int>& idx);

// Hover readout helper: returns true when the mouse is inside the plot area.
bool plotHovered(ImPlotPoint* p);

}  // namespace fr
