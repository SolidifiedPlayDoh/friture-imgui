#include "ui/levels_panel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "config.hpp"
#include "imgui.h"

namespace fr {

namespace {

constexpr double kTickSeconds = 0.025;   // Friture's display tick
constexpr double kLabelSeconds = 0.25;   // numeric readout refresh
constexpr double kW = 0.65;
constexpr double kRmsResponse = 0.300;

ImU32 col(unsigned rgb) {
    return IM_COL32((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff, 255);
}

ImU32 iecColor(double iec) {
    if (iec > 0.8) return col(0xF00014);
    if (iec > 0.7) return col(0xF0A014);
    if (iec > 0.6) return col(0xDCDC14);
    if (iec > 0.4) return col(0xA0DC14);
    if (iec > 0.2) return col(0x28A028);
    return col(0x20B020);
}

std::string levelText(double dB) {
    if (dB < -150.0) return "-inf";
    char b[32];
    std::snprintf(b, sizeof b, "%.1f", dB);
    return b;
}

}  // namespace

void LevelsPanel::onData(const Chunk& chunk, double dt) {
    channels_ = chunk.channels;
    const double n = kRmsResponse * kSampleRate;
    const double alpha = 1.0 - std::pow(1.0 - kW, 1.0 / (n + 1.0));
    const double n2 = 0.025 / kTickSeconds;
    const double alpha2 = 1.0 - std::pow(1.0 - kW, 1.0 / (n2 + 1.0));

    for (int c = 0; c < channels_; ++c) {
        Channel& s = ch_[c];
        const std::vector<double>& y = chunk.ch[c];
        for (double v : y) {
            s.oldRms = alpha * (v * v) + (1.0 - alpha) * s.oldRms;
            s.pendingMax = std::max(s.pendingMax, std::fabs(v));
        }
        s.levelRms = 10.0 * std::log10(std::max(s.oldRms, 1e-300));
    }

    // Peak value and ballistics advance in fixed 25 ms steps, as upstream.
    stepAcc_ += dt;
    while (stepAcc_ >= kTickSeconds) {
        stepAcc_ -= kTickSeconds;
        for (int c = 0; c < channels_; ++c) {
            Channel& s = ch_[c];
            if (s.pendingMax > s.oldMax * (1.0 - alpha2)) s.oldMax = s.pendingMax;
            else s.oldMax *= (1.0 - alpha2);
            s.pendingMax = 0.0;
            s.levelMax = 20.0 * std::log10(std::max(s.oldMax, 1e-300));
            s.ballistic.update(dBtoIEC(std::max(s.levelMax, s.levelRms)));
        }
    }

    labelAcc_ += dt;
    if (labelAcc_ >= kLabelSeconds) {
        labelAcc_ = std::fmod(labelAcc_, kLabelSeconds);
        for (int c = 0; c < channels_; ++c) {
            ch_[c].slowMax = ch_[c].levelMax;
            ch_[c].slowRms = ch_[c].levelRms;
        }
    }
}

void LevelsPanel::drawMeter(const Channel& ch, float x, float top, float w, float h) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float bottom = top + h;
    dl->AddRectFilled(ImVec2(x, top), ImVec2(x + w, bottom), col(0x0c0c0c));

    auto bar = [&](double iec, ImU32 cTop, ImU32 cMid, ImU32 cBot) {
        float bh = (float)std::clamp(iec, 0.0, 1.0) * h;
        if (bh < 1.f) return;
        // A static vertical gradient clipped to the bar height.
        dl->PushClipRect(ImVec2(x, bottom - bh), ImVec2(x + w, bottom), true);
        float mid = top + h * 0.5f;
        dl->AddRectFilledMultiColor(ImVec2(x, top), ImVec2(x + w, mid), cTop, cTop, cMid, cMid);
        dl->AddRectFilledMultiColor(ImVec2(x, mid), ImVec2(x + w, bottom), cMid, cMid, cBot, cBot);
        dl->PopClipRect();
    };

    // Peak bar: red at the top, through amber/yellow, to green.
    {
        float bh = (float)std::clamp(dBtoIEC(ch.levelMax), 0.0, 1.0) * h;
        if (bh >= 1.f) {
            dl->PushClipRect(ImVec2(x, bottom - bh), ImVec2(x + w, bottom), true);
            struct Stop { float p; unsigned c; };
            const Stop stops[] = {{0.0f, 0xF00014}, {0.2f, 0xF00014}, {0.3f, 0xF0A014},
                                  {0.4f, 0xDCDC14}, {0.6f, 0xA0DC14}, {0.8f, 0x28A028},
                                  {1.0f, 0x28A028}};
            for (int i = 0; i + 1 < 7; ++i) {
                float y0 = top + h * stops[i].p, y1 = top + h * stops[i + 1].p;
                if (y1 <= y0) continue;
                dl->AddRectFilledMultiColor(ImVec2(x, y0), ImVec2(x + w, y1), col(stops[i].c),
                                            col(stops[i].c), col(stops[i + 1].c),
                                            col(stops[i + 1].c));
            }
            dl->PopClipRect();
        }
    }
    // RMS bar on top, blue.
    bar(dBtoIEC(ch.levelRms), col(0xE6E6FF), col(0x0000FF), col(0x000096));

    // Ballistic peak line.
    double b = ch.ballistic.value();
    float y = std::round(bottom - (float)b * h);
    dl->AddLine(ImVec2(x, y), ImVec2(x + w, y), iecColor(b), 2.f);
    dl->AddRect(ImVec2(x, top), ImVec2(x + w, bottom), col(0x2c2c2c));
}

void LevelsPanel::draw() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float fh = ImGui::GetFontSize();
    const float lineH = ImGui::GetTextLineHeightWithSpacing();

    // Header text: peak and RMS readouts.
    float y = origin.y;
    auto readout = [&](const char* title, double a, double b) {
        char buf[64];
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
        if (channels_ == 2) {
            std::snprintf(buf, sizeof buf, "1: %s", levelText(a).c_str());
            ImGui::TextUnformatted(buf);
            std::snprintf(buf, sizeof buf, "2: %s", levelText(b).c_str());
            ImGui::TextUnformatted(buf);
        } else {
            ImGui::TextUnformatted(levelText(a).c_str());
        }
        ImGui::TextDisabled("dB FS");
        ImGui::TextDisabled("%s", title);
        y = ImGui::GetCursorScreenPos().y + 4.f;
    };
    readout("Peak", ch_[0].slowMax, ch_[1].slowMax);
    readout("RMS", ch_[0].slowRms, ch_[1].slowRms);

    // Meters fill the remaining height.
    const float top = y + 4.f;
    const float bottom = origin.y + avail.y - 4.f;
    const float h = std::max(40.f, bottom - top);
    const float scaleW = fh * 2.4f;
    const float mw = std::max(14.f, fh * 1.1f);

    // Scale ticks.
    for (int db = 0; db >= -70; db -= 10) {
        float ty = top + h * (1.f - (float)dBtoIEC(db));
        char b[8];
        std::snprintf(b, sizeof b, "%d", db);
        ImVec2 ts = ImGui::CalcTextSize(b);
        dl->AddText(ImVec2(origin.x + scaleW - ts.x - 4.f, ty - ts.y * 0.5f),
                    IM_COL32(154, 154, 154, 255), b);
        dl->AddLine(ImVec2(origin.x + scaleW - 2.f, ty), ImVec2(origin.x + scaleW, ty),
                    IM_COL32(90, 90, 90, 255));
    }

    float x = origin.x + scaleW + 2.f;
    for (int c = 0; c < channels_; ++c) {
        drawMeter(ch_[c], x, top, mw, h);
        x += mw + 4.f;
    }
    (void)lineH;
    ImGui::Dummy(avail);
}

}  // namespace fr
