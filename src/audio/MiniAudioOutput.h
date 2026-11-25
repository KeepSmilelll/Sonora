#pragma once

#include "audio/IAudioOutput.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace sonora {

class MiniAudioOutput final : public IAudioOutput {
public:
    MiniAudioOutput();
    ~MiniAudioOutput() override;

    MiniAudioOutput(const MiniAudioOutput&) = delete;
    MiniAudioOutput& operator=(const MiniAudioOutput&) = delete;

    QVector<AudioDeviceInfo> enumerateDevices() const override;

    bool open(const OutputConfig& config,
              int requestedSampleRate,
              int requestedChannels,
              AudioRenderCallback callback,
              ActiveOutputStatus* statusOut = nullptr) override;

    bool start() override;
    void stop() override;
    void close() override;
    bool isOpen() const override;
    bool isStarted() const override;

    ActiveOutputStatus activeStatus() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sonora
