#pragma once
// Audio I/O built on miniaudio: device enumeration, lock-free capture, and a
// playback device that mixes any number of OutputSource objects.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace fr {

struct DeviceInfo {
    std::string name;
    bool isDefault = false;
    int maxChannels = 2;
};

// Producers of output audio (e.g. signal generator). mixInto() runs on
// the audio thread and must *add* into the interleaved buffer.
class OutputSource {
public:
    virtual ~OutputSource() = default;
    virtual void mixInto(float* interleaved, uint32_t frames, uint32_t channels) = 0;
    // True while the source still wants the output device running.
    virtual bool wantsOutput() const = 0;
};

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    bool init(std::string* err = nullptr);
    void shutdown();

    void refreshDevices();
    const std::vector<DeviceInfo>& inputDevices() const;
    const std::vector<DeviceInfo>& outputDevices() const;

    // ---- capture
    bool openInput(int index, std::string* err = nullptr);
    int inputIndex() const;
    int inputChannels() const;
    bool inputRunning() const;
    void startInput();
    void stopInput();
    // Pull everything captured since the last call as planar floats for the
    // chosen channel(s). Returns the number of frames.
    size_t fetchInput(int firstChannel, int secondChannel, bool duo,
                      std::vector<float>& a, std::vector<float>& b);
    uint64_t overflowCount() const;

    // ---- playback
    void addOutputSource(OutputSource* src);
    bool openOutput(int index, std::string* err = nullptr);
    int outputIndex() const;
    int outputChannels() const;
    // Call once per frame: starts/stops the output device as sources need it.
    void updateOutput();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace fr
