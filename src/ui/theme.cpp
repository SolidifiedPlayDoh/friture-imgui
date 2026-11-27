#include "ui/theme.hpp"

#include "imgui.h"
#include "implot.h"

namespace fr {

namespace {
inline ImVec4 hex(unsigned rgb, float a = 1.f) {
    return ImVec4(((rgb >> 16) & 0xff) / 255.f, ((rgb >> 8) & 0xff) / 255.f,
                  (rgb & 0xff) / 255.f, a);
}
}  // namespace

void applyTheme(float dpi) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    s.WindowRounding = 0.f;
    s.ChildRounding = 0.f;
    s.FrameRounding = 2.f;
    s.PopupRounding = 2.f;
    s.GrabRounding = 2.f;
    s.ScrollbarRounding = 2.f;
    s.TabRounding = 0.f;
    s.WindowBorderSize = 1.f;
    s.FrameBorderSize = 0.f;
    s.PopupBorderSize = 1.f;
    s.WindowPadding = ImVec2(8, 8);
    s.FramePadding = ImVec2(8, 5);
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.ScrollbarSize = 12.f;
    s.GrabMinSize = 10.f;
    s.WindowMenuButtonPosition = ImGuiDir_None;
    s.DockingSeparatorSize = 2.f;
    s.ScaleAllSizes(dpi);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text] = hex(0xe6e6e6);
    c[ImGuiCol_TextDisabled] = hex(0x6a6a6a);
    c[ImGuiCol_WindowBg] = hex(0x0f0f0f);
    c[ImGuiCol_ChildBg] = hex(0x0f0f0f);
    c[ImGuiCol_PopupBg] = hex(0x1b1b1b);
    c[ImGuiCol_Border] = hex(0x2c2c2c);
    c[ImGuiCol_BorderShadow] = hex(0x000000, 0.f);
    c[ImGuiCol_FrameBg] = hex(0x252525);
    c[ImGuiCol_FrameBgHovered] = hex(0x303030);
    c[ImGuiCol_FrameBgActive] = hex(0x3a3a3a);
    c[ImGuiCol_TitleBg] = hex(0x181818);
    c[ImGuiCol_TitleBgActive] = hex(0x1d1d1d);
    c[ImGuiCol_TitleBgCollapsed] = hex(0x181818);
    c[ImGuiCol_MenuBarBg] = hex(0x181818);
    c[ImGuiCol_ScrollbarBg] = hex(0x0f0f0f);
    c[ImGuiCol_ScrollbarGrab] = hex(0x363636);
    c[ImGuiCol_ScrollbarGrabHovered] = hex(0x444444);
    c[ImGuiCol_ScrollbarGrabActive] = hex(0x555555);
    c[ImGuiCol_CheckMark] = hex(0x2a7de1);
    c[ImGuiCol_SliderGrab] = hex(0x2a7de1);
    c[ImGuiCol_SliderGrabActive] = hex(0x4a95ee);
    c[ImGuiCol_Button] = hex(0x2b2b2b);
    c[ImGuiCol_ButtonHovered] = hex(0x363636);
    c[ImGuiCol_ButtonActive] = hex(0x222222);
    c[ImGuiCol_Header] = hex(0x2b2b2b);
    c[ImGuiCol_HeaderHovered] = hex(0x363636);
    c[ImGuiCol_HeaderActive] = hex(0x2a7de1, 0.7f);
    c[ImGuiCol_Separator] = hex(0x2c2c2c);
    c[ImGuiCol_SeparatorHovered] = hex(0x2a7de1);
    c[ImGuiCol_SeparatorActive] = hex(0x4a95ee);
    c[ImGuiCol_ResizeGrip] = hex(0x2b2b2b);
    c[ImGuiCol_ResizeGripHovered] = hex(0x2a7de1);
    c[ImGuiCol_ResizeGripActive] = hex(0x4a95ee);
    c[ImGuiCol_Tab] = hex(0x181818);
    c[ImGuiCol_TabHovered] = hex(0x2a7de1, 0.8f);
    c[ImGuiCol_TabSelected] = hex(0x2b2b2b);
    c[ImGuiCol_TabSelectedOverline] = hex(0x2a7de1);
    c[ImGuiCol_TabDimmed] = hex(0x141414);
    c[ImGuiCol_TabDimmedSelected] = hex(0x1d1d1d);
    c[ImGuiCol_DockingPreview] = hex(0x2a7de1, 0.4f);
    c[ImGuiCol_DockingEmptyBg] = hex(0x0a0a0a);
    c[ImGuiCol_TextSelectedBg] = hex(0x2a7de1, 0.4f);
    c[ImGuiCol_NavCursor] = hex(0x2a7de1);
    c[ImGuiCol_PlotLines] = hex(0x27e0b8);
    c[ImGuiCol_PlotHistogram] = hex(0x2a7de1);

    ImPlotStyle& p = ImPlot::GetStyle();
    p.PlotPadding = ImVec2(6 * dpi, 6 * dpi);
    p.PlotBorderSize = 1.f;
    p.MinorAlpha = 0.5f;
    p.Colors[ImPlotCol_PlotBg] = hex(0x0c0c0c);
    p.Colors[ImPlotCol_PlotBorder] = hex(0x3c3c3c);
    p.Colors[ImPlotCol_FrameBg] = hex(0x0f0f0f);
    p.Colors[ImPlotCol_LegendBg] = hex(0x1b1b1b, 0.9f);
    p.Colors[ImPlotCol_AxisText] = hex(0x9a9a9a);
    p.Colors[ImPlotCol_AxisGrid] = hex(0x343434);
    p.Colors[ImPlotCol_AxisTick] = hex(0x5a5a5a);
    p.Colors[ImPlotCol_AxisBg] = hex(0, 0.f);
}

}  // namespace fr
