#pragma once
// Small ImGui form helpers for the settings popups.

#include <algorithm>
#include <cstdio>

#include "imgui.h"

namespace fr::ui {

inline void label(const char* text) {
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(text);
    ImGui::SameLine(ImGui::GetFontSize() * 10.f);
}

// Combo box over string items. Returns true when the selection changed.
inline bool combo(const char* lbl, int& index, const char* const* items, int count) {
    label(lbl);
    ImGui::SetNextItemWidth(-FLT_MIN);
    bool changed = false;
    ImGui::PushID(lbl);
    if (ImGui::BeginCombo("##c", items[std::clamp(index, 0, count - 1)])) {
        for (int i = 0; i < count; ++i)
            if (ImGui::Selectable(items[i], i == index)) {
                if (i != index) changed = true;
                index = i;
            }
        ImGui::EndCombo();
    }
    ImGui::PopID();
    return changed;
}

// Integer spin box (drag + ctrl-click to type), with unit suffix.
inline bool spinInt(const char* lbl, int& v, int lo, int hi, const char* suffix = "",
                    float speed = 1.f) {
    label(lbl);
    ImGui::SetNextItemWidth(-FLT_MIN);
    char fmt[32];
    std::snprintf(fmt, sizeof fmt, "%%d%s", suffix);
    ImGui::PushID(lbl);
    bool ch = ImGui::DragInt("##i", &v, speed, lo, hi, fmt, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    return ch;
}

inline bool spinDouble(const char* lbl, double& v, double lo, double hi, const char* fmt,
                       float speed = 0.1f) {
    label(lbl);
    ImGui::SetNextItemWidth(-FLT_MIN);
    float f = (float)v;
    ImGui::PushID(lbl);
    bool ch = ImGui::DragFloat("##d", &f, speed, (float)lo, (float)hi, fmt, ImGuiSliderFlags_AlwaysClamp);
    ImGui::PopID();
    if (ch) v = f;
    return ch;
}

inline bool check(const char* lbl, bool& v) {
    label(lbl);
    ImGui::PushID(lbl);
    bool ch = ImGui::Checkbox("##k", &v);
    ImGui::PopID();
    return ch;
}

}  // namespace fr::ui
