#pragma once

#include <memory>
#include <string>
#include <vector>

#include "audio/demo_source.hpp"
#include "audio/engine.hpp"
#include "audio/generator.hpp"
#include "audio/player.hpp"
#include "settings.hpp"
#include "ui/levels_panel.hpp"
#include "widgets/widget.hpp"

struct GLFWwindow;

namespace fr {

class App {
public:
    App();
    ~App();
    bool init();
    void run();

    // Debug / verification helpers (command line).
    void setScreenshot(const std::string& path) { screenshotPath_ = path; }
    void setScreenshotFrames(int n) { screenshotFrames_ = n; }
    void setBenchSeconds(double s) { benchSeconds_ = s; }
    void setNoSave(bool b) { noSave_ = b; }
    void setListDevices(bool b) { listDevices_ = b; }
    void setDemo(bool b) { demoCli_ = b; }

private:
    void saveScreenshot(const std::string& path);
    std::string screenshotPath_;
    int screenshotFrames_ = 90;
    double benchSeconds_ = 0;
    bool noSave_ = false;
    bool listDevices_ = false;
    bool demoCli_ = false;

    struct Dock {
        int id = 0;
        WidgetType type = WidgetType::FFTSpectrum;
        std::unique_ptr<Widget> widget;
        bool open = true;
    };

    // frame pieces
    void feedAudio(double dt);
    void drawMainMenuBar();
    void drawPlaybackBar(float height);
    void drawStatusBar(float height);
    void drawDock(Dock& d);
    void drawLevels();
    void drawAudioSettings();
    void drawAbout();
    void buildDefaultLayout(unsigned dockspaceId);

    // state management
    Dock& addDock(WidgetType type, int id = -1);
    void reconfigureInput();
    void openSavedInput();
    void loadSettings();
    void saveSettings();
    void applyDisplaySettings();
    void limitFrameRate(double frameStart);

    GLFWwindow* window_ = nullptr;
    Context ctx_;
    AudioEngine engine_;
    Player player_;
    SignalGenerator generator_;
    LevelsPanel levels_;
    Settings settings_;
    std::string configPath_, iniPath_;
    bool freshLayout_ = false;

    std::vector<std::unique_ptr<Dock>> docks_;
    int nextDockId_ = 1;

    // audio config
    int firstChannel_ = 0, secondChannel_ = 1;
    bool duo_ = false;
    int historySeconds_ = 30;
    bool showPlayback_ = true;
    std::string savedInputName_, savedOutputName_;
    std::string audioError_;
    bool demo_ = false;
    DemoSource demoSrc_;
    double demoCarry_ = 0.0;
    Chunk chunk_;
    std::vector<float> fa_, fb_;

    // playback transport
    float playStart_ = 0.f;  // slider value, seconds (<= 0)
    float playPos_ = 0.f;

    // display
    bool vsync_ = true;
    int fpsLimitIndex_ = 0;
    bool showFps_ = true;
    float uiScale_ = 1.f;
    float pendingUiScale_ = 1.f;
    float baseScale_ = 1.f;

    bool showAudioSettings_ = false;
    bool showAbout_ = false;
    double lastFrameTime_ = 0;
};

}  // namespace fr
