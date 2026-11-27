#pragma once
// Records the live input into a history buffer and plays it back to the output
// device (port of friture/playback/player.py).

#include <atomic>
#include <mutex>
#include <vector>

#include "audio/engine.hpp"
#include "audio/ringbuffer.hpp"

namespace fr {

class Player : public OutputSource {
public:
    Player();

    void setHistorySeconds(int seconds);
    int historySeconds() const { return historySec_; }

    // Feed live samples (main thread). b may be empty for mono.
    void push(const float* a, const float* b, size_t n);

    // Seconds currently recorded (<= history length).
    double recordedSeconds() const;

    // `startTime`: 0 plays from the start of the history; negative values are
    // an offset in seconds back from the end.
    void play(double startTime);
    void stop();
    bool playing() const { return playing_.load(); }
    // True once after playback ran to the end on its own.
    bool takeFinished() { return finished_.exchange(false); }
    // Playback position relative to the end of the recording (<= 0, seconds).
    double playbackTime() const;

    // OutputSource
    void mixInto(float* interleaved, uint32_t frames, uint32_t channels) override;
    bool wantsOutput() const override { return playing_.load(); }

private:
    mutable std::mutex mu_;
    std::vector<double> scratch_;
    RingBuffer buf_;
    int channels_ = 1;
    int historySec_ = 30;
    int64_t recorded_ = 0;
    int64_t playOffset_ = 0;
    std::atomic<bool> playing_{false};
    std::atomic<bool> finished_{false};
};

}  // namespace fr
