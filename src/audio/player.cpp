#include "audio/player.hpp"

#include <algorithm>

#include "config.hpp"

namespace fr {

Player::Player() { buf_.configure(1, (size_t)historySec_ * kSampleRate); }

void Player::setHistorySeconds(int seconds) {
    std::lock_guard<std::mutex> lk(mu_);
    historySec_ = std::max(1, seconds);
    buf_.growIfNeeded((size_t)historySec_ * kSampleRate);
    recorded_ = std::min<int64_t>(recorded_, (int64_t)historySec_ * kSampleRate);
}

void Player::push(const float* a, const float* b, size_t n) {
    if (n == 0) return;
    std::lock_guard<std::mutex> lk(mu_);
    int ch = b ? 2 : 1;
    if (ch != channels_) {  // channel count changed: start a fresh history
        channels_ = ch;
        buf_.configure(ch, (size_t)historySec_ * kSampleRate);
        recorded_ = 0;
    }
    const float* planar[2] = {a, b};
    buf_.push(planar, n);
    recorded_ = std::min<int64_t>(recorded_ + (int64_t)n, (int64_t)historySec_ * kSampleRate);
}

double Player::recordedSeconds() const {
    std::lock_guard<std::mutex> lk(mu_);
    return (double)recorded_ / kSampleRate;
}

void Player::play(double startTime) {
    std::lock_guard<std::mutex> lk(mu_);
    if (playing_.load() || recorded_ == 0) return;
    if (startTime == 0.0) {
        playOffset_ = buf_.offset() - recorded_;
    } else {
        int64_t startOffset = std::max<int64_t>(-recorded_, (int64_t)(startTime * kSampleRate));
        playOffset_ = buf_.offset() + startOffset;
    }
    finished_.store(false);
    playing_.store(true);
}

void Player::stop() {
    std::lock_guard<std::mutex> lk(mu_);
    playing_.store(false);
}

double Player::playbackTime() const {
    std::lock_guard<std::mutex> lk(mu_);
    return (double)(playOffset_ - buf_.offset()) / kSampleRate;
}

void Player::mixInto(float* out, uint32_t frames, uint32_t channels) {
    if (!playing_.load()) return;
    std::unique_lock<std::mutex> lk(mu_, std::try_to_lock);
    if (!lk.owns_lock()) return;  // main thread is writing; skip this tiny slice

    int64_t available = buf_.offset() - playOffset_;
    int64_t toCopy = std::min<int64_t>(available, frames);
    if (toCopy > 0) {
        if ((int64_t)scratch_.size() < toCopy) scratch_.resize((size_t)toCopy);  // grows only on the first large callbacks
        double* tmp_ = scratch_.data();
        const int inCh = buf_.channels();
        for (int c = 0; c < inCh; ++c) {
            buf_.copy(c, playOffset_ + toCopy, (size_t)toCopy, tmp_);
            // mono plays on every output channel; stereo maps 1:1 (extra channels silent)
            for (uint32_t oc = 0; oc < channels; ++oc) {
                bool take = (inCh == 1) || ((int)oc == c);
                if (!take) continue;
                for (int64_t i = 0; i < toCopy; ++i)
                    out[(size_t)i * channels + oc] += (float)tmp_[i];
            }
        }
        playOffset_ += toCopy;
    }
    if (available < (int64_t)frames) {
        playing_.store(false);
        finished_.store(true);
    }
}

}  // namespace fr
