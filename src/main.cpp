#include <cstdlib>
#include <cstring>

#include "app.hpp"

int main(int argc, char** argv) {
    fr::App app;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--screenshot") && i + 1 < argc) app.setScreenshot(argv[++i]);
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) app.setScreenshotFrames(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--bench") && i + 1 < argc) app.setBenchSeconds(std::atof(argv[++i]));
        else if (!std::strcmp(argv[i], "--no-save")) app.setNoSave(true);
        else if (!std::strcmp(argv[i], "--list-devices")) app.setListDevices(true);
        else if (!std::strcmp(argv[i], "--demo")) app.setDemo(true);
    }
    if (!app.init()) return 1;
    app.run();
    return 0;
}
