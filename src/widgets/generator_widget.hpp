#pragma once
// UI for the shared signal generator (sine, white/pink noise, sweep, burst).

#include "widgets/widget.hpp"

namespace fr {

class GeneratorWidget : public Widget {
public:
    explicit GeneratorWidget(Context& ctx) : Widget(ctx) {}
    const char* name() const override { return "Generator"; }
    void draw(ImVec2 size) override;
    void drawSettings() override;
};

}  // namespace fr
