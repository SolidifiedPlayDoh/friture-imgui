#pragma once
// Oscilloscope with leading-edge trigger (port of friture/scope.py).

#include <vector>

#include "widgets/widget.hpp"

namespace fr {

class ScopeWidget : public Widget {
public:
    explicit ScopeWidget(Context& ctx) : Widget(ctx) {}
    const char* name() const override { return "Scope"; }
    void onData(const Chunk& chunk) override;
    void draw(ImVec2 size) override;
    void drawSettings() override;
    void save(Settings& s, const std::string& p) const override;
    void load(const Settings& s, const std::string& p) override;

private:
    double timeRange_ = 50.0;  // ms
    std::vector<double> t_, y1_, y2_, buf0_, buf1_;
    bool two_ = false;
    std::vector<double> dx_, dy_, dy2_;
};

}  // namespace fr
