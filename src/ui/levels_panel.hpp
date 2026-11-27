#pragma once
// Peak / RMS level meters with IEC ballistics (port of friture/levels.py + Levels.qml).

#include "dsp/iec.hpp"
#include "widgets/widget.hpp"

namespace fr {

class LevelsPanel {
public:
    explicit LevelsPanel(Context&) {}
    // `dt` is the time since the previous call, in seconds.
    void onData(const Chunk& chunk, double dt);
    void draw();

private:
    struct Channel {
        double oldRms = 1e-30;
        double oldMax = 1e-30;
        double pendingMax = 0.0;
        double levelRms = -200.0;
        double levelMax = -200.0;
        double slowRms = -200.0;
        double slowMax = -200.0;
        BallisticPeak ballistic;
    };
    void drawMeter(const Channel& ch, float x, float top, float width, float height);

    Channel ch_[2];
    int channels_ = 1;
    double stepAcc_ = 0.0;
    double labelAcc_ = 0.0;
};

}  // namespace fr
