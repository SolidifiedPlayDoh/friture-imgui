#pragma once
// RMS level history over minutes (port of friture/longlevels.py).

#include <memory>
#include <vector>

#include "audio/ringbuffer.hpp"
#include "dsp/filters.hpp"
#include "widgets/widget.hpp"

namespace fr {

class LongLevelsWidget : public Widget {
public:
    explicit LongLevelsWidget(Context& ctx);
    const char* name() const override { return "Long Time Levels"; }
    void onData(const Chunk& chunk) override;
    void draw(ImVec2 size) override;
    void drawSettings() override;
    void save(Settings& s, const std::string& p) const override;
    void load(const Settings& s, const std::string& p) override;
    void onStreamReset() override;

private:
    struct FirStage {
        LFilter f;
        std::vector<double> tmp;
    };
    void setResponse();
    void setDuration();
    void pushSubsampled(const std::vector<double>& x, std::vector<double>& out);

    int levelMin_ = -70, levelMax_ = -20;
    int responseSec_ = 20;
    int maxTime_ = 600;

    int ndec_ = 0;
    double subRate_ = 1.0;
    int lengthSamples_ = 1;
    std::vector<FirStage> stages_;
    LFilter gauss_;
    RingBuffer levels_;
    int64_t oldIndex_ = 0;
    std::vector<double> in_, a_, b_, xs_, ys_;
};

}  // namespace fr
