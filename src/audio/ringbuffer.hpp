#pragma once
// Multi-channel history ring buffer with an absolute, monotonically growing
// sample offset (same indexing model as Friture's RingBuffer).

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fr {

class RingBuffer {
public:
    RingBuffer() { configure(1, 1 << 16); }

    // (Re)allocate, discarding contents. Offset restarts at 0.
    void configure(int channels, size_t minCapacity) {
        channels_ = channels;
        cap_ = pow2(minCapacity);
        buf_.assign(channels_, std::vector<float>(cap_, 0.f));
        offset_ = 0;
    }

    // Grow capacity, preserving the most recent samples and the offset.
    void growIfNeeded(size_t minCapacity) {
        size_t want = pow2(minCapacity);
        if (want <= cap_) return;
        std::vector<std::vector<float>> nb(channels_, std::vector<float>(want, 0.f));
        int64_t avail = (int64_t)std::min<uint64_t>((uint64_t)offset_, cap_);
        for (int c = 0; c < channels_; ++c)
            for (int64_t i = offset_ - avail; i < offset_; ++i)
                nb[c][(size_t)i & (want - 1)] = buf_[c][(size_t)i & (cap_ - 1)];
        buf_.swap(nb);
        cap_ = want;
    }

    int channels() const { return channels_; }
    size_t capacity() const { return cap_; }
    int64_t offset() const { return offset_; }

    // planar[c] points to n samples for channel c.
    void push(const float* const* planar, size_t n) {
        for (int c = 0; c < channels_; ++c) {
            const float* src = planar[c];
            std::vector<float>& dst = buf_[c];
            size_t start = (size_t)offset_ & (cap_ - 1);
            size_t first = std::min(n, cap_ - start);
            std::copy(src, src + first, dst.begin() + start);
            if (first < n) std::copy(src + first, src + n, dst.begin());
        }
        offset_ += (int64_t)n;
    }

    // Copy the `len` samples of channel `ch` that end at absolute index
    // `endIndex` (exclusive). Anything not available reads as zero.
    void copy(int ch, int64_t endIndex, size_t len, double* out) const {
        int64_t start = endIndex - (int64_t)len;
        int64_t oldest = offset_ - (int64_t)cap_;
        const std::vector<float>& b = buf_[ch];
        for (size_t i = 0; i < len; ++i) {
            int64_t idx = start + (int64_t)i;
            out[i] = (idx < 0 || idx < oldest || idx >= offset_)
                         ? 0.0 : (double)b[(size_t)idx & (cap_ - 1)];
        }
    }

    void copyLatest(int ch, size_t len, double* out) const { copy(ch, offset_, len, out); }

private:
    static size_t pow2(size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }
    int channels_ = 1;
    size_t cap_ = 0;
    int64_t offset_ = 0;
    std::vector<std::vector<float>> buf_;
};

// A chunk of freshly captured audio handed to every widget once per frame.
struct Chunk {
    int channels = 1;
    std::vector<double> ch[2];
    size_t frames() const { return ch[0].size(); }
};

}  // namespace fr
