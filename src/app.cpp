#include "app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <thread>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"
#include "implot.h"

#include "config.hpp"
#include "ui/controls.hpp"
#include "ui/theme.hpp"

#ifdef __APPLE__
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

namespace fr {

namespace {

constexpr int kFpsLimits[] = {0, 30, 60, 90, 120, 144, 165, 240};
const char* const kFpsLimitNames[] = {"Unlimited", "30", "60", "90", "120", "144", "165", "240"};

void glfwError(int code, const char* msg) { std::fprintf(stderr, "GLFW error %d: %s\n", code, msg); }

}  // namespace

App::App() : levels_(ctx_) {}

App::~App() {
    if (window_) {
        if (!noSave_) saveSettings();
        engine_.shutdown();
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
    }
}

// ----------------------------------------------------------------- init

bool App::init() {
    configPath_ = configDir() + "/settings.ini";
    iniPath_ = configDir() + "/layout.ini";
    freshLayout_ = !std::filesystem::exists(iniPath_);
    settings_.load(configPath_);

    glfwSetErrorCallback(glfwError);
    if (!glfwInit()) return false;
#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    const char* glslVersion = "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    const char* glslVersion = "#version 330";
#endif
    int w = settings_.getInt("window.w", 1500), h = settings_.getInt("window.h", 900);
    window_ = glfwCreateWindow(w, h, "Friture", nullptr, nullptr);
    if (!window_) return false;
    glfwMakeContextCurrent(window_);

    loadSettings();
    glfwSwapInterval(vsync_ ? 1 : 0);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = iniPath_.c_str();

    baseScale_ = ImGui_ImplGlfw_GetContentScaleForWindow(window_);
    if (!(baseScale_ > 0.f)) baseScale_ = 1.f;
    ImFont* font = nullptr;
    if (std::filesystem::exists(FRITURE_FONT_PATH))
        font = io.Fonts->AddFontFromFileTTF(FRITURE_FONT_PATH, 16.0f);
    if (!font) io.Fonts->AddFontDefault();
    applyDisplaySettings();

    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    // audio
    std::string err;
    if (!engine_.init(&err)) audioError_ = err;
    ctx_.engine = &engine_;
    ctx_.generator = &generator_;
    ctx_.ring.configure(1, 1 << 20);
    engine_.addOutputSource(&generator_);
    openSavedInput();

    if (listDevices_) {
        for (int i = 0; i < (int)engine_.inputDevices().size(); ++i) {
            const auto& d = engine_.inputDevices()[i];
            std::printf("in  [%d] %s%s  (%d ch)%s\n", i, d.name.c_str(), d.isDefault ? " [default]" : "", d.maxChannels, i == engine_.inputIndex() ? "  <- opened" : "");
        }
        for (int i = 0; i < (int)engine_.outputDevices().size(); ++i) {
            const auto& d = engine_.outputDevices()[i];
            std::printf("out [%d] %s%s  (%d ch)%s\n", i, d.name.c_str(), d.isDefault ? " [default]" : "", d.maxChannels, i == engine_.outputIndex() ? "  <- opened" : "");
        }
        return false;
    }

    // docks
    std::string list = settings_.getString("docks", "");
    std::stringstream ss(list);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (tok.empty()) continue;
        int id = std::atoi(tok.c_str());
        int type = settings_.getInt("dock." + tok + ".type", (int)WidgetType::FFTSpectrum);
        if (type < 0 || type >= (int)WidgetType::Count) type = (int)WidgetType::FFTSpectrum;
        Dock& d = addDock((WidgetType)type, id);
        d.widget->load(settings_, "dock." + tok + ".w.");
    }
    if (docks_.empty()) {
        addDock(WidgetType::FFTSpectrum);
        addDock(WidgetType::Spectrogram);
        addDock(WidgetType::OctaveSpectrum);
        freshLayout_ = true;
    }
    return true;
}

void App::applyDisplaySettings() {
    applyTheme(baseScale_ * uiScale_);
    ImGui::GetStyle().FontScaleMain = baseScale_ * uiScale_;
}

App::Dock& App::addDock(WidgetType type, int id) {
    auto d = std::make_unique<Dock>();
    if (id < 0) id = nextDockId_;
    nextDockId_ = std::max(nextDockId_, id + 1);
    d->id = id;
    d->type = type;
    d->widget = createWidget(type, ctx_);
    docks_.push_back(std::move(d));
    return *docks_.back();
}

// ----------------------------------------------------------------- settings

void App::loadSettings() {
    vsync_ = settings_.getBool("ui.vsync", true);
    fpsLimitIndex_ = std::clamp(settings_.getInt("ui.fpslimit", 0), 0, 7);
    showFps_ = settings_.getBool("ui.showfps", true);
    uiScale_ = pendingUiScale_ = (float)settings_.getDouble("ui.scale", 1.0);
    firstChannel_ = settings_.getInt("audio.first", 0);
    secondChannel_ = settings_.getInt("audio.second", 1);
    demo_ = settings_.getBool("audio.demo", false) || demoCli_;
    duo_ = settings_.getBool("audio.duo", false);
    savedInputName_ = settings_.getString("audio.indevice", "");
    savedOutputName_ = settings_.getString("audio.outdevice", "");
    generator_.kind.store(settings_.getInt("gen.kind", 0));
    generator_.sineFrequency.store(settings_.getDouble("gen.sine", 440.0));
    generator_.sweepStart.store(settings_.getDouble("gen.sweepstart", 20.0));
    generator_.sweepStop.store(settings_.getDouble("gen.sweepstop", 22000.0));
    generator_.sweepPeriod.store(settings_.getDouble("gen.sweepperiod", 1.0));
    generator_.burstPeriod.store(settings_.getDouble("gen.burstperiod", 1.0));
    generator_.volumeDb.store(settings_.getDouble("gen.volume", -12.0));
}

void App::saveSettings() {
    int w = 0, h = 0;
    glfwGetWindowSize(window_, &w, &h);
    settings_.set("window.w", w);
    settings_.set("window.h", h);
    settings_.set("ui.vsync", vsync_);
    settings_.set("ui.fpslimit", fpsLimitIndex_);
    settings_.set("ui.showfps", showFps_);
    settings_.set("ui.scale", (double)uiScale_);
    settings_.set("audio.first", firstChannel_);
    settings_.set("audio.second", secondChannel_);
    settings_.set("audio.duo", duo_);
    settings_.set("audio.demo", demo_);
    const auto& ins = engine_.inputDevices();
    if (engine_.inputIndex() >= 0 && engine_.inputIndex() < (int)ins.size())
        settings_.set("audio.indevice", ins[engine_.inputIndex()].name);
    const auto& outs = engine_.outputDevices();
    if (engine_.outputIndex() >= 0 && engine_.outputIndex() < (int)outs.size())
        settings_.set("audio.outdevice", outs[engine_.outputIndex()].name);
    settings_.set("gen.kind", generator_.kind.load());
    settings_.set("gen.sine", generator_.sineFrequency.load());
    settings_.set("gen.sweepstart", generator_.sweepStart.load());
    settings_.set("gen.sweepstop", generator_.sweepStop.load());
    settings_.set("gen.sweepperiod", generator_.sweepPeriod.load());
    settings_.set("gen.burstperiod", generator_.burstPeriod.load());
    settings_.set("gen.volume", generator_.volumeDb.load());

    settings_.erasePrefix("dock.");
    std::string list;
    for (auto& d : docks_) {
        std::string id = std::to_string(d->id);
        list += id + ",";
        settings_.set("dock." + id + ".type", (int)d->type);
        d->widget->save(settings_, "dock." + id + ".w.");
    }
    settings_.set("docks", list);
    settings_.save(configPath_);
}

void App::openSavedInput() {
    const auto& ins = engine_.inputDevices();
    std::vector<int> order;
    for (int i = 0; i < (int)ins.size(); ++i)
        if (!savedInputName_.empty() && ins[i].name == savedInputName_) order.push_back(i);
    for (int i = 0; i < (int)ins.size(); ++i)
        if (ins[i].isDefault && std::find(order.begin(), order.end(), i) == order.end()) order.push_back(i);
    for (int i = 0; i < (int)ins.size(); ++i)
        if (std::find(order.begin(), order.end(), i) == order.end()) order.push_back(i);
    audioError_.clear();
    bool ok = demo_;
    for (int i : order) {
        if (demo_) break;
        std::string err;
        if (engine_.openInput(i, &err)) { ok = true; break; }
        audioError_ = err;
    }
    if (!ok && audioError_.empty()) audioError_ = "No audio input devices found";
    if (demo_) engine_.stopInput();
    reconfigureInput();

    const auto& outs = engine_.outputDevices();
    order.clear();
    for (int i = 0; i < (int)outs.size(); ++i)
        if (!savedOutputName_.empty() && outs[i].name == savedOutputName_) order.push_back(i);
    for (int i = 0; i < (int)outs.size(); ++i)
        if (outs[i].isDefault && std::find(order.begin(), order.end(), i) == order.end()) order.push_back(i);
    for (int i = 0; i < (int)outs.size(); ++i)
        if (std::find(order.begin(), order.end(), i) == order.end()) order.push_back(i);
    for (int i : order)
        if (engine_.openOutput(i)) break;
}

void App::reconfigureInput() {
    int nch = demo_ ? 2 : std::max(1, engine_.inputChannels());
    firstChannel_ = std::clamp(firstChannel_, 0, nch - 1);
    secondChannel_ = std::clamp(secondChannel_, 0, nch - 1);
    ctx_.ring.configure(1, 1 << 20);
    ctx_.channels = 1;
    for (auto& d : docks_) d->widget->onStreamReset();
}

// ----------------------------------------------------------------- audio feed

void App::feedAudio(double dt) {
    if (!ctx_.running) return;
    if (!demo_ && !engine_.inputRunning()) return;
    const bool duo = duo_ && (demo_ || engine_.inputChannels() > 1);
    size_t n = 0;
    if (demo_) {
        demoCarry_ += dt * kSampleRate;
        n = (size_t)demoCarry_;
        demoCarry_ -= (double)n;
        demoSrc_.generate(n, fa_, fb_);
    } else {
        n = engine_.fetchInput(firstChannel_, secondChannel_, duo, fa_, fb_);
    }

    const int ch = duo ? 2 : 1;
    if (ch != ctx_.channels) {
        ctx_.channels = ch;
        ctx_.ring.configure(ch, 1 << 20);
        for (auto& d : docks_) d->widget->onStreamReset();
    }

    chunk_.channels = ch;
    chunk_.ch[0].assign(fa_.begin(), fa_.end());
    if (duo) chunk_.ch[1].assign(fb_.begin(), fb_.end());
    else chunk_.ch[1].clear();

    if (n > 0) {
        const float* planar[2] = {fa_.data(), duo ? fb_.data() : fa_.data()};
        ctx_.ring.push(planar, n);
    }

    levels_.onData(chunk_, dt);
    for (auto& d : docks_) d->widget->onData(chunk_);
}

// ----------------------------------------------------------------- UI

void App::drawMainMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    bool running = ctx_.running;
    if (ImGui::Button(running ? "Stop" : "Start")) {
        if (running) {
            engine_.stopInput();
            ctx_.running = false;
        } else {
            if (!demo_) engine_.startInput();
            ctx_.running = true;
            for (auto& d : docks_) d->widget->onStreamReset();
        }
    }

    if (ImGui::BeginMenu("Add widget")) {
        for (int t = 0; t < (int)WidgetType::Count; ++t)
            if (ImGui::MenuItem(widgetTypeName((WidgetType)t))) addDock((WidgetType)t);
        ImGui::EndMenu();
    }
    if (ImGui::Button("Settings")) showAudioSettings_ = !showAudioSettings_;
    if (ImGui::Button("About")) showAbout_ = true;

    if (showFps_) {
        char b[64];
        std::snprintf(b, sizeof b, "%.0f FPS", ImGui::GetIO().Framerate);
        float tw = ImGui::CalcTextSize(b).x;
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - tw - ImGui::GetStyle().WindowPadding.x * 2);
        ImGui::TextDisabled("%s", b);
    }
    ImGui::EndMainMenuBar();
}

void App::drawStatusBar(float height) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiWindowFlags f = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoDocking;
    if (!ImGui::BeginViewportSideBar("##status", vp, ImGuiDir_Down, height, f)) {
        ImGui::End();
        return;
    }
    const auto& ins = engine_.inputDevices();
    if (demo_) {
        ImGui::TextDisabled("Built-in test signal  |  2 ch  |  %d Hz", kSampleRate);
    } else if (!audioError_.empty()) {
        ImGui::TextColored(ImVec4(0.94f, 0.4f, 0.3f, 1.f), "%s", audioError_.c_str());
    } else if (engine_.inputIndex() >= 0 && engine_.inputIndex() < (int)ins.size()) {
        ImGui::TextDisabled("%s  |  %d ch  |  %d Hz  |  %llu overruns",
                            ins[engine_.inputIndex()].name.c_str(), engine_.inputChannels(),
                            kSampleRate, (unsigned long long)engine_.overflowCount());
    }
    ImGui::End();
}

void App::drawLevels() {
    ImGui::SetNextWindowSize(ImVec2(130, 400), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    if (ImGui::Begin("Levels###levels", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        levels_.draw();
    ImGui::End();
    ImGui::PopStyleVar();
}

void App::drawDock(Dock& d) {
    char title[96];
    std::snprintf(title, sizeof title, "%s###dock%d", d.widget->name(), d.id);
    ImGui::SetNextWindowSize(ImVec2(560, 360), ImGuiCond_FirstUseEver);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    bool visible = ImGui::Begin(title, &d.open,
                                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    if (visible) {
        // header row: widget type and settings
        int type = (int)d.type;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 11.f);
        if (ImGui::BeginCombo("##type", widgetTypeName(d.type))) {
            for (int t = 0; t < (int)WidgetType::Count; ++t)
                if (ImGui::Selectable(widgetTypeName((WidgetType)t), t == type) && t != type) {
                    d.type = (WidgetType)t;
                    d.widget = createWidget(d.type, ctx_);
                }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("Settings")) ImGui::OpenPopup("##wsettings");
        ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetFontSize() * 26.f, 0), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::BeginPopup("##wsettings")) {
            d.widget->drawSettings();
            ImGui::EndPopup();
        }
        ImVec2 avail = ImGui::GetContentRegionAvail();
        d.widget->draw(avail);
    }
    ImGui::End();
}

void App::drawAudioSettings() {
    ImGui::SetNextWindowSize(ImVec2(520, 0), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Settings", &showAudioSettings_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }
    ImGui::SeparatorText("Audio input");
    const auto& ins = engine_.inputDevices();
    int cur = engine_.inputIndex();
    ui::label("Input device");
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* curName = demo_ ? "Built-in test signal (demo)"
                          : (cur >= 0 && cur < (int)ins.size() ? ins[cur].name.c_str() : "(none)");
    if (ImGui::BeginCombo("##indev", curName)) {
        if (ImGui::Selectable("Built-in test signal (demo)", demo_) && !demo_) {
            demo_ = true;
            engine_.stopInput();
            audioError_.clear();
            ctx_.running = true;
            reconfigureInput();
        }
        for (int i = 0; i < (int)ins.size(); ++i) {
            std::string nm = ins[i].name + (ins[i].isDefault ? "  (default)" : "");
            if (ImGui::Selectable(nm.c_str(), !demo_ && i == cur) && (demo_ || i != cur)) {
                std::string err;
                if (engine_.openInput(i, &err)) {
                    demo_ = false;
                    audioError_.clear();
                    ctx_.running = true;
                    reconfigureInput();
                } else {
                    audioError_ = err;
                }
            }
        }
        ImGui::EndCombo();
    }

    int nch = demo_ ? 2 : std::max(1, engine_.inputChannels());
    std::vector<std::string> chNames;
    std::vector<const char*> chPtrs;
    for (int i = 0; i < nch; ++i) chNames.push_back("Channel " + std::to_string(i + 1));
    for (auto& s : chNames) chPtrs.push_back(s.c_str());

    int mode = duo_ ? 1 : 0;
    const char* const modes[] = {"Single input", "Two inputs (stereo)"};
    if (ui::combo("Input mode", mode, modes, nch > 1 ? 2 : 1)) duo_ = mode == 1;
    ui::combo(duo_ ? "First channel" : "Channel", firstChannel_, chPtrs.data(), nch);
    if (duo_ && nch > 1) ui::combo("Second channel", secondChannel_, chPtrs.data(), nch);

    ImGui::SeparatorText("Audio output (generator)");
    const auto& outs = engine_.outputDevices();
    int ocur = engine_.outputIndex();
    ui::label("Output device");
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##outdev", ocur >= 0 && ocur < (int)outs.size() ? outs[ocur].name.c_str() : "(none)")) {
        for (int i = 0; i < (int)outs.size(); ++i) {
            std::string nm = outs[i].name + (outs[i].isDefault ? "  (default)" : "");
            if (ImGui::Selectable(nm.c_str(), i == ocur) && i != ocur) engine_.openOutput(i);
        }
        ImGui::EndCombo();
    }

    ImGui::SeparatorText("Display");
    ui::check("VSync", vsync_);
    glfwSwapInterval(vsync_ ? 1 : 0);
    if (!vsync_) ui::combo("Frame rate limit", fpsLimitIndex_, kFpsLimitNames, 8);
    ui::check("Show FPS", showFps_);
    ui::label("UI scale");
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::SliderFloat("##scale", &pendingUiScale_, 0.75f, 2.0f, "%.2fx");
    if (!ImGui::IsItemActive() && pendingUiScale_ != uiScale_) {
        uiScale_ = pendingUiScale_;
        applyDisplaySettings();
    }

    if (ImGui::Button("Refresh devices")) engine_.refreshDevices();
    ImGui::End();
}

void App::drawAbout() {
    if (showAbout_) ImGui::OpenPopup("About Friture");
    if (ImGui::BeginPopupModal("About Friture", &showAbout_, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Friture (Dear ImGui)  v%s", FRITURE_VERSION);
        ImGui::Separator();
        ImGui::TextWrapped(
            "A C++ / Dear ImGui port of Friture, the real-time audio analyzer.");
        ImGui::Spacing();
        ImGui::TextWrapped(
            "Derived from Friture by Timothee Lecomte and contributors "
            "(https://friture.org), licensed under the GNU GPL v3.");
        ImGui::Spacing();
        if (ImGui::Button("Close")) {
            showAbout_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void App::buildDefaultLayout(unsigned dockspaceId) {
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNode(dockspaceId);
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, vp->WorkSize);

    auto name = [&](size_t i) { return "###dock" + std::to_string(docks_[i]->id); };

    ImGuiID rest = 0;
    ImGuiID left = ImGui::DockBuilderSplitNode(dockspaceId, ImGuiDir_Left, 0.11f, nullptr, &rest);
    ImGui::DockBuilderDockWindow("Levels###levels", left);

    const size_t n = docks_.size();
    if (n == 3) {
        // The classic arrangement: one wide analyzer on top, two below.
        ImGuiID top = 0;
        ImGuiID bottom = ImGui::DockBuilderSplitNode(rest, ImGuiDir_Down, 0.45f, nullptr, &top);
        ImGuiID bl = 0;
        ImGuiID br = ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.5f, nullptr, &bl);
        ImGui::DockBuilderDockWindow(name(0).c_str(), top);
        ImGui::DockBuilderDockWindow(name(1).c_str(), bl);
        ImGui::DockBuilderDockWindow(name(2).c_str(), br);
    } else if (n > 0) {
        // A near-square grid for any other count.
        const int cols = (int)std::ceil(std::sqrt((double)n));
        const int rows = (int)std::ceil((double)n / cols);
        ImGuiID remaining = rest;
        size_t idx = 0;
        for (int r = 0; r < rows; ++r) {
            ImGuiID rowNode = remaining;
            if (r < rows - 1) {
                ImGuiID after = 0;
                rowNode = ImGui::DockBuilderSplitNode(remaining, ImGuiDir_Up, 1.0f / (rows - r), nullptr, &after);
                remaining = after;
            }
            int inRow = (int)std::min<size_t>(cols, n - idx);
            ImGuiID rowRemaining = rowNode;
            for (int c = 0; c < inRow; ++c) {
                ImGuiID cell = rowRemaining;
                if (c < inRow - 1) {
                    ImGuiID after = 0;
                    cell = ImGui::DockBuilderSplitNode(rowRemaining, ImGuiDir_Left, 1.0f / (inRow - c), nullptr, &after);
                    rowRemaining = after;
                }
                ImGui::DockBuilderDockWindow(name(idx++).c_str(), cell);
            }
        }
    }
    ImGui::DockBuilderFinish(dockspaceId);
}

// ----------------------------------------------------------------- loop

void App::limitFrameRate(double frameStart) {
    if (vsync_ || fpsLimitIndex_ == 0) return;
    const double target = 1.0 / kFpsLimits[fpsLimitIndex_];
    while (glfwGetTime() - frameStart < target - 0.0015)
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    while (glfwGetTime() - frameStart < target) { /* spin the last bit */ }
}

void App::run() {
    lastFrameTime_ = glfwGetTime();
    bool layoutBuilt = false;
    const double runStart = glfwGetTime();
    long frameCount = 0;

    while (!glfwWindowShouldClose(window_)) {
        const double frameStart = glfwGetTime();
        glfwPollEvents();
        if (glfwGetWindowAttrib(window_, GLFW_ICONIFIED)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        const double dt = std::min(frameStart - lastFrameTime_, 0.25);
        lastFrameTime_ = frameStart;

        feedAudio(dt);
        engine_.updateOutput();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        drawMainMenuBar();
        const float barH = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 1.2f;
        drawStatusBar(barH);

        ImGuiID dockspace = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        if (freshLayout_ && !layoutBuilt) {
            buildDefaultLayout(dockspace);
            layoutBuilt = true;
            freshLayout_ = false;
        }

        drawLevels();
        for (auto& d : docks_) drawDock(*d);
        docks_.erase(std::remove_if(docks_.begin(), docks_.end(),
                                    [](const std::unique_ptr<Dock>& d) { return !d->open; }),
                     docks_.end());

        if (showAudioSettings_) drawAudioSettings();
        drawAbout();

        ImGui::Render();
        int fw, fh;
        glfwGetFramebufferSize(window_, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.04f, 0.04f, 0.04f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        ++frameCount;
        if (!screenshotPath_.empty() && frameCount == screenshotFrames_) {
            saveScreenshot(screenshotPath_);
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }
        glfwSwapBuffers(window_);

        if (benchSeconds_ > 0 && glfwGetTime() - runStart >= benchSeconds_) {
            double secs = glfwGetTime() - runStart;
            std::printf("bench: %ld frames in %.2f s = %.1f FPS (vsync=%d)\n", frameCount, secs,
                        frameCount / secs, (int)vsync_);
            glfwSetWindowShouldClose(window_, GLFW_TRUE);
        }

        limitFrameRate(frameStart);
    }
}

void App::saveScreenshot(const std::string& path) {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    std::vector<unsigned char> px((size_t)w * h * 3), flipped((size_t)w * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; ++y)
        std::copy(px.begin() + (size_t)(h - 1 - y) * w * 3, px.begin() + (size_t)(h - y) * w * 3,
                  flipped.begin() + (size_t)y * w * 3);
    std::string ppm = path + ".ppm";
    if (FILE* f = std::fopen(ppm.c_str(), "wb")) {
        std::fprintf(f, "P6\n%d %d\n255\n", w, h);
        std::fwrite(flipped.data(), 1, flipped.size(), f);
        std::fclose(f);
        std::string cmd = "sips -s format png \"" + ppm + "\" --out \"" + path + "\" >/dev/null 2>&1 && rm \"" + ppm + "\"";
        if (std::system(cmd.c_str()) != 0) std::fprintf(stderr, "screenshot conversion failed\n");
    }
}

}  // namespace fr
