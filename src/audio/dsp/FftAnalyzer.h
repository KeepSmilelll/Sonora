#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace virela {

// Zero-allocation lock-free FFT audio tap & logarithmic spectrum analyzer.
// RT thread pushes mono-summed PCM frames into a ring window; UI/render thread
// calls computeBands() only when the visualizer is visible.
class FftAnalyzer {
public:
    static constexpr size_t kFftSize = 1024;
    static constexpr size_t kBandCount = 48;

    FftAnalyzer();

    void setEnabled(bool enabled) noexcept { enabled_.store(enabled, std::memory_order_release); }
    bool isEnabled() const noexcept { return enabled_.load(std::memory_order_acquire); }

    void setSampleRate(int sampleRate) noexcept {
        sampleRate_.store(sampleRate > 0 ? sampleRate : 48000, std::memory_order_release);
    }

    // RT-thread only: lock-free, allocation-free tap of interleaved float PCM
    void pushInterleaved(const float* interleaved, size_t frameCount, uint32_t channels) noexcept;

    // UI/render thread: computes 48 normalized [0.0, 1.0] logarithmic frequency bands
    // and fills smoothed bar heights + peak hold values.
    void computeBands(std::array<float, kBandCount>& outBars,
                      std::array<float, kBandCount>& outPeaks,
                      bool isPlaying);

    // Pure algorithmic helper exposed for deterministic unit testing
    static void computeMagnitudeSpectrum(const std::array<float, kFftSize>& timeSamples,
                                         std::array<float, kFftSize / 2>& outMagnitudes);

    void reset() noexcept;

private:
    std::atomic<bool> enabled_{false};
    std::atomic<int> sampleRate_{48000};

    // Double-buffered snapshot written by RT thread whenever kFftSize samples accumulate
    std::array<float, kFftSize> rtAccum_{};
    size_t rtAccumPos_ = 0;

    std::array<std::array<float, kFftSize>, 2> snapshots_{};
    std::atomic<int> readyIdx_{0};
    std::atomic<uint64_t> snapshotSeq_{0};
    uint64_t lastConsumedSeq_ = 0;

    std::array<float, kFftSize> hannWindow_{};
    std::array<float, kBandCount> smoothedBars_{};
    std::array<float, kBandCount> peakHold_{};
    std::array<float, kBandCount> peakVelocity_{};
};

} // namespace virela
