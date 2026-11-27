#pragma once

#include <QString>
#include <array>
#include <atomic>
#include <cstdint>

namespace virela {

// 10-band parametric equalizer with double-precision Direct Form II Transposed biquad state
// and lock-free atomic coefficient swapping for real-time audio callbacks.
class BiquadEq {
public:
    static constexpr size_t kBandCount = 10;
    static constexpr size_t kMaxChannels = 8;
    static constexpr std::array<double, kBandCount> kFrequencies = {
        31.25, 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0
    };

    struct Coeffs {
        double b0 = 1.0;
        double b1 = 0.0;
        double b2 = 0.0;
        double a1 = 0.0;
        double a2 = 0.0;
    };

    struct Preset {
        QString id;
        QString name;
        double preampDb = 0.0;
        std::array<double, kBandCount> gainsDb{};
    };

    BiquadEq();

    // Control-thread API (thread-safe with RT thread via double-buffered snapshot)
    void setEnabled(bool enabled);
    bool isEnabled() const noexcept { return enabled_.load(std::memory_order_acquire); }

    void setSampleRate(double sampleRate);
    void setPreampDb(double preampDb);
    double preampDb() const noexcept { return preampDb_; }

    void setBandGainDb(size_t bandIndex, double gainDb);
    void setAllBandGainsDb(const std::array<double, kBandCount>& gainsDb);
    std::array<double, kBandCount> bandGainsDb() const noexcept { return gainsDb_; }

    // Evaluates theoretical magnitude response in dB at frequency fHz (for UI EQ curve rendering)
    double magnitudeDbAt(double fHz) const;

    // Built-in audiophile presets
    static const std::array<Preset, 7>& builtInPresets();

    // RT-thread only: resets biquad delay lines (call on seek / sample rate change / stop)
    void resetState() noexcept;

    // RT-thread only: processes interleaved float32 samples in-place (zero locks, zero allocs)
    void processInterleaved(float* interleaved, size_t frameCount, uint32_t channels) noexcept;

private:
    struct ParamSet {
        double preampLinear = 1.0;
        std::array<Coeffs, kBandCount> bands{};
    };

    struct FilterState {
        double z1 = 0.0;
        double z2 = 0.0;
    };

    static Coeffs designPeakingFilter(double freqHz, double sampleRate, double q, double gainDb);
    static Coeffs designLowShelf(double freqHz, double sampleRate, double slope, double gainDb);
    static Coeffs designHighShelf(double freqHz, double sampleRate, double slope, double gainDb);
    void rebuildAndPublish();

    std::atomic<bool> enabled_{false};
    double sampleRate_ = 48000.0;
    double preampDb_ = 0.0;
    std::array<double, kBandCount> gainsDb_{};

    // Double-buffered coefficient sets swapped atomically
    std::array<ParamSet, 2> buffers_{};
    std::atomic<int> activeIdx_{0};
    int rtLastIdx_ = 0;

    // RT-only filter state: [band][channel]
    std::array<std::array<FilterState, kMaxChannels>, kBandCount> state_{};
};

} // namespace virela
