#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_DECODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#include "audio/engine.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>

#include "config.hpp"
#include "miniaudio.h"

namespace fr {

namespace {

struct InputState {
    ma_device dev{};
    ma_pcm_rb rb{};
    uint32_t channels = 0;
    bool devInit = false;
    bool rbInit = false;
    std::atomic<uint64_t> overflow{0};
};

void captureCallback(ma_device* dev, void*, const void* in, ma_uint32 frames) {
    auto* st = static_cast<InputState*>(dev->pUserData);
    const float* src = static_cast<const float*>(in);
    ma_uint32 remaining = frames;
    while (remaining > 0) {
        ma_uint32 n = remaining;
        void* dst = nullptr;
        if (ma_pcm_rb_acquire_write(&st->rb, &n, &dst) != MA_SUCCESS || n == 0) {
            st->overflow.fetch_add(1, std::memory_order_relaxed);
            break;
        }
        std::memcpy(dst, src, (size_t)n * st->channels * sizeof(float));
        ma_pcm_rb_commit_write(&st->rb, n);
        src += (size_t)n * st->channels;
        remaining -= n;
    }
}

constexpr int kMaxSources = 8;

struct OutputState {
    ma_device dev{};
    bool devInit = false;
    bool running = false;
    uint32_t channels = 0;
    OutputSource* sources[kMaxSources] = {};
    std::atomic<int> sourceCount{0};
};

void playbackCallback(ma_device* dev, void* out, const void*, ma_uint32 frames) {
    auto* st = static_cast<OutputState*>(dev->pUserData);
    float* o = static_cast<float*>(out);
    std::memset(o, 0, (size_t)frames * dev->playback.channels * sizeof(float));
    int n = st->sourceCount.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) st->sources[i]->mixInto(o, frames, dev->playback.channels);
}

int queryMaxChannels(ma_context* ctx, ma_device_type type, const ma_device_id* id) {
    ma_device_info info;
    if (ma_context_get_device_info(ctx, type, id, &info) != MA_SUCCESS) return 2;
    int best = 0;
    for (ma_uint32 i = 0; i < info.nativeDataFormatCount; ++i)
        best = std::max<int>(best, (int)info.nativeDataFormats[i].channels);
    return best > 0 ? std::min(best, 64) : 2;
}

}  // namespace

struct AudioEngine::Impl {
    ma_context ctx{};
    bool ctxInit = false;

    std::vector<DeviceInfo> inInfos, outInfos;
    std::vector<ma_device_id> inIds, outIds;

    std::unique_ptr<InputState> in;
    int inIndex = -1;
    bool inRunning = false;
    std::vector<float> scratch;

    OutputState out;
    int outIndex = -1;
    std::chrono::steady_clock::time_point lastWanted{};

    void closeInput() {
        if (!in) return;
        if (in->devInit) ma_device_uninit(&in->dev);
        if (in->rbInit) ma_pcm_rb_uninit(&in->rb);
        in.reset();
        inRunning = false;
    }
    void closeOutput() {
        if (out.devInit) {
            ma_device_uninit(&out.dev);
            out.devInit = false;
            out.running = false;
        }
    }
};

AudioEngine::AudioEngine() : impl_(new Impl) {}
AudioEngine::~AudioEngine() { shutdown(); }

bool AudioEngine::init(std::string* err) {
    if (ma_context_init(nullptr, 0, nullptr, &impl_->ctx) != MA_SUCCESS) {
        if (err) *err = "Failed to initialise the audio context";
        return false;
    }
    impl_->ctxInit = true;
    refreshDevices();
    return true;
}

void AudioEngine::shutdown() {
    if (!impl_) return;
    impl_->closeInput();
    impl_->closeOutput();
    if (impl_->ctxInit) {
        ma_context_uninit(&impl_->ctx);
        impl_->ctxInit = false;
    }
}

void AudioEngine::refreshDevices() {
    ma_device_info *pb = nullptr, *cap = nullptr;
    ma_uint32 pbN = 0, capN = 0;
    if (ma_context_get_devices(&impl_->ctx, &pb, &pbN, &cap, &capN) != MA_SUCCESS) return;
    impl_->inInfos.clear(); impl_->inIds.clear();
    impl_->outInfos.clear(); impl_->outIds.clear();
    for (ma_uint32 i = 0; i < capN; ++i) {
        DeviceInfo d;
        d.name = cap[i].name;
        d.isDefault = cap[i].isDefault != 0;
        d.maxChannels = queryMaxChannels(&impl_->ctx, ma_device_type_capture, &cap[i].id);
        impl_->inInfos.push_back(d);
        impl_->inIds.push_back(cap[i].id);
    }
    for (ma_uint32 i = 0; i < pbN; ++i) {
        DeviceInfo d;
        d.name = pb[i].name;
        d.isDefault = pb[i].isDefault != 0;
        d.maxChannels = queryMaxChannels(&impl_->ctx, ma_device_type_playback, &pb[i].id);
        impl_->outInfos.push_back(d);
        impl_->outIds.push_back(pb[i].id);
    }
}

const std::vector<DeviceInfo>& AudioEngine::inputDevices() const { return impl_->inInfos; }
const std::vector<DeviceInfo>& AudioEngine::outputDevices() const { return impl_->outInfos; }

// ---------------------------------------------------------------- capture

bool AudioEngine::openInput(int index, std::string* err) {
    if (index < 0 || index >= (int)impl_->inIds.size()) {
        if (err) *err = "No such input device";
        return false;
    }
    impl_->closeInput();

    auto st = std::make_unique<InputState>();
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &impl_->inIds[index];
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 0;  // native channel count
    cfg.sampleRate = kSampleRate;
    cfg.periodSizeInFrames = kFramesPerBuffer;
    cfg.dataCallback = captureCallback;
    cfg.pUserData = st.get();
    if (ma_device_init(&impl_->ctx, &cfg, &st->dev) != MA_SUCCESS) {
        if (err) *err = "Could not open input device '" + impl_->inInfos[index].name + "'";
        return false;
    }
    st->devInit = true;
    st->channels = st->dev.capture.channels;
    if (ma_pcm_rb_init(ma_format_f32, st->channels, kSampleRate * 3, nullptr, nullptr, &st->rb) !=
        MA_SUCCESS) {
        ma_device_uninit(&st->dev);
        st->devInit = false;
        if (err) *err = "Could not allocate the capture buffer";
        return false;
    }
    st->rbInit = true;
    if (ma_device_start(&st->dev) != MA_SUCCESS) {
        ma_device_uninit(&st->dev);
        ma_pcm_rb_uninit(&st->rb);
        if (err) *err = "Could not start input device '" + impl_->inInfos[index].name + "'";
        return false;
    }
    impl_->in = std::move(st);
    impl_->inIndex = index;
    impl_->inRunning = true;
    return true;
}

int AudioEngine::inputIndex() const { return impl_->inIndex; }
int AudioEngine::inputChannels() const { return impl_->in ? (int)impl_->in->channels : 0; }
bool AudioEngine::inputRunning() const { return impl_->inRunning; }

void AudioEngine::startInput() {
    auto& in = impl_->in;
    if (!in || impl_->inRunning) return;
    ma_pcm_rb_reset(&in->rb);
    if (ma_device_start(&in->dev) == MA_SUCCESS) impl_->inRunning = true;
}

void AudioEngine::stopInput() {
    auto& in = impl_->in;
    if (!in || !impl_->inRunning) return;
    ma_device_stop(&in->dev);
    impl_->inRunning = false;
}

uint64_t AudioEngine::overflowCount() const {
    return impl_->in ? impl_->in->overflow.load() : 0;
}

size_t AudioEngine::fetchInput(int c0, int c1, bool duo, std::vector<float>& a,
                               std::vector<float>& b) {
    a.clear();
    b.clear();
    auto& in = impl_->in;
    if (!in || !impl_->inRunning) return 0;
    const uint32_t nch = in->channels;
    c0 = std::clamp(c0, 0, (int)nch - 1);
    c1 = std::clamp(c1, 0, (int)nch - 1);

    for (;;) {
        ma_uint32 n = 4096;
        void* src = nullptr;
        if (ma_pcm_rb_acquire_read(&in->rb, &n, &src) != MA_SUCCESS || n == 0) break;
        const float* p = static_cast<const float*>(src);
        size_t base = a.size();
        a.resize(base + n);
        if (duo) b.resize(base + n);
        for (ma_uint32 i = 0; i < n; ++i) {
            a[base + i] = p[(size_t)i * nch + c0];
            if (duo) b[base + i] = p[(size_t)i * nch + c1];
        }
        ma_pcm_rb_commit_read(&in->rb, n);
    }
    return a.size();
}

// ---------------------------------------------------------------- playback

void AudioEngine::addOutputSource(OutputSource* src) {
    int n = impl_->out.sourceCount.load();
    if (n >= kMaxSources) return;
    impl_->out.sources[n] = src;
    impl_->out.sourceCount.store(n + 1, std::memory_order_release);
}

bool AudioEngine::openOutput(int index, std::string* err) {
    if (index < 0 || index >= (int)impl_->outIds.size()) {
        if (err) *err = "No such output device";
        return false;
    }
    bool wasRunning = impl_->out.running;
    impl_->closeOutput();

    ma_device_config cfg = ma_device_config_init(ma_device_type_playback);
    cfg.playback.pDeviceID = &impl_->outIds[index];
    cfg.playback.format = ma_format_f32;
    cfg.playback.channels = 0;
    cfg.sampleRate = kSampleRate;
    cfg.periodSizeInFrames = 1024;
    cfg.dataCallback = playbackCallback;
    cfg.pUserData = &impl_->out;
    if (ma_device_init(&impl_->ctx, &cfg, &impl_->out.dev) != MA_SUCCESS) {
        if (err) *err = "Could not open output device '" + impl_->outInfos[index].name + "'";
        return false;
    }
    impl_->out.devInit = true;
    impl_->out.channels = impl_->out.dev.playback.channels;
    impl_->outIndex = index;
    if (wasRunning && ma_device_start(&impl_->out.dev) == MA_SUCCESS) impl_->out.running = true;
    return true;
}

int AudioEngine::outputIndex() const { return impl_->outIndex; }
int AudioEngine::outputChannels() const { return (int)impl_->out.channels; }

void AudioEngine::updateOutput() {
    OutputState& o = impl_->out;
    if (!o.devInit) return;
    bool wants = false;
    int n = o.sourceCount.load(std::memory_order_acquire);
    for (int i = 0; i < n; ++i) wants = wants || o.sources[i]->wantsOutput();

    auto now = std::chrono::steady_clock::now();
    if (wants) {
        impl_->lastWanted = now;
        if (!o.running && ma_device_start(&o.dev) == MA_SUCCESS) o.running = true;
    } else if (o.running && now - impl_->lastWanted > std::chrono::milliseconds(250)) {
        ma_device_stop(&o.dev);
        o.running = false;
    }
}

}  // namespace fr
