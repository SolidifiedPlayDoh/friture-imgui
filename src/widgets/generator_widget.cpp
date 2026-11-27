#include "widgets/generator_widget.hpp"

#include <algorithm>
#include <cmath>

#include "audio/engine.hpp"
#include "audio/generator.hpp"
#include "ui/controls.hpp"

namespace fr {

void GeneratorWidget::draw(ImVec2 size) {
    SignalGenerator& g = *ctx_.generator;

    int kind = g.kind.load();
    const char* names[] = {"Sine", "White noise", "Pink noise", "Sweep", "Burst"};
    if (ui::combo("Signal", kind, names, 5)) g.kind.store(kind);

    switch (kind) {
        case (int)GeneratorKind::Sine: {
            double f = g.sineFrequency.load();
            if (ui::spinDouble("Frequency", f, 1.0, 24000.0, "%.1f Hz", 1.f)) g.sineFrequency.store(f);
            break;
        }
        case (int)GeneratorKind::Sweep: {
            double f1 = g.sweepStart.load(), f2 = g.sweepStop.load(), per = g.sweepPeriod.load();
            if (ui::spinDouble("Start frequency", f1, 1.0, 24000.0, "%.0f Hz", 1.f)) g.sweepStart.store(f1);
            if (ui::spinDouble("Stop frequency", f2, 1.0, 24000.0, "%.0f Hz", 10.f)) g.sweepStop.store(f2);
            if (ui::spinDouble("Period", per, 0.1, 100.0, "%.2f s", 0.05f)) g.sweepPeriod.store(per);
            break;
        }
        case (int)GeneratorKind::Burst: {
            double per = g.burstPeriod.load();
            if (ui::spinDouble("Period", per, 0.01, 100.0, "%.2f s", 0.05f)) g.burstPeriod.store(per);
            break;
        }
        default: break;
    }

    double vol = g.volumeDb.load();
    if (ui::spinDouble("Volume", vol, -60.0, 0.0, "%.0f dB", 0.5f)) g.volumeDb.store(vol);

    ImGui::Spacing();
    bool playing = g.isPlaying();
    ImGui::PushStyleColor(ImGuiCol_Button, playing ? ImVec4(0.55f, 0.18f, 0.18f, 1.f) : ImVec4(0.16f, 0.49f, 0.88f, 1.f));
    if (ImGui::Button(playing ? "Stop" : "Start", ImVec2(std::min(size.x, ImGui::GetFontSize() * 10.f), 0)))
        g.setPlaying(!playing);
    ImGui::PopStyleColor();
    if (!ctx_.engine || ctx_.engine->outputIndex() < 0) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.94f, 0.4f, 0.3f, 1.f), "No output device");
    }
}

void GeneratorWidget::drawSettings() {
    ImGui::TextDisabled("The generator plays on the output device chosen in Settings.");
}

}  // namespace fr
