#pragma once

#include "audio/AudioTypes.h"

#include <QVector>
#include <cstdint>
#include <functional>

namespace virela {

// Real-time audio callback: fills interleaved float32 output with frameCount frames.
using AudioRenderCallback = std::function<void(float* interleavedOut, uint32_t frameCount, uint32_t channels)>;

class IAudioOutput {
public:
    virtual ~IAudioOutput() = default;

    virtual QVector<AudioDeviceInfo> enumerateDevices() const = 0;

    virtual bool open(const OutputConfig& config,
                      int requestedSampleRate,
                      int requestedChannels,
                      AudioRenderCallback callback,
                      ActiveOutputStatus* statusOut = nullptr) = 0;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual bool isStarted() const = 0;

    virtual ActiveOutputStatus activeStatus() const = 0;
};

} // namespace virela
