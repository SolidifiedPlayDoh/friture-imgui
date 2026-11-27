#pragma once
// Delay estimator between two channels using GCC-PHAT (port of friture/delay_estimator.py).

#include <vector>

#include "audio/ringbuffer.hpp"
#include "dsp/filters.hpp"
#include "widgets/widget.hpp"

namespace fr {

class DelayWidget : public Widget {
public:
    explicit DelayWidget(Context& ctx);
    const char* name() const override { return "Delay Estimator"; }
    void onData(const Chunk& chunk) override;
    void draw(ImVec2 size) override;
    void drawSettings() override;
    void save(Settings& s, const std::string& p) const override;
    void load(const Settings& s, const std::string& p) override;

private:
    void setup();
    void estimate(int64_t index);

    static constexpr int kNdec = 2;
    Decimator dec0_[kNdec], dec1_[kNdec];
    RingBuffer r0_, r1_;
    double subRate_ = (double)48000 / 4;
    double delayRange_ = 1.0;  // seconds
    int64_t oldIndex_ = 0;
    bool two_ = false;

    std::vector<double> d0_, d1_, oldX_, smoothed_, tmpA_, tmpB_;
    double delayMs_ = 0, distanceM_ = 0, extremum_ = 0;
    int correlation_ = 0;
};

}  // namespace fr
