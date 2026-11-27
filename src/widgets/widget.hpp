#pragma once
// Base class for every dockable analyzer, plus the shared application context.

#include <memory>
#include <string>

#include "audio/ringbuffer.hpp"
#include "imgui.h"
#include "settings.hpp"

namespace fr {

class AudioEngine;
class SignalGenerator;

// State shared by all widgets.
struct Context {
    RingBuffer ring;          // live input history
    AudioEngine* engine = nullptr;
    SignalGenerator* generator = nullptr;
    int channels = 1;         // 1 or 2 active input channels
    bool running = true;      // live capture running
    float uiScale = 1.f;
};

class Widget {
public:
    explicit Widget(Context& ctx) : ctx_(ctx) {}
    virtual ~Widget() = default;

    virtual const char* name() const = 0;

    // Called every frame with the newly captured samples (may be empty).
    virtual void onData(const Chunk& chunk) {}
    // Draw the analyzer into the given content region.
    virtual void draw(ImVec2 size) = 0;
    // Contents of the settings popup.
    virtual void drawSettings() {}

    virtual void save(Settings& s, const std::string& p) const {}
    virtual void load(const Settings& s, const std::string& p) {}

    // Live capture was paused/resumed or the stream was reset.
    virtual void onStreamReset() {}

protected:
    Context& ctx_;
};

enum class WidgetType {
    Scope = 0,
    FFTSpectrum,
    Spectrogram,
    OctaveSpectrum,
    Generator,
    DelayEstimator,
    LongLevels,
    PitchTracker,
    Count
};

const char* widgetTypeName(WidgetType t);
std::unique_ptr<Widget> createWidget(WidgetType t, Context& ctx);

}  // namespace fr
