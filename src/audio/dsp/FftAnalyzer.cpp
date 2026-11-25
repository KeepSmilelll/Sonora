#include "audio/dsp/FftAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace sonora {

namespace {
constexpr double kPi = 3.14159265358979323846;

void fftRadix2(std::array<std::complex<float>, FftAnalyzer::kFftSize>& a) noexcept {
    constexpr size_t N = FftAnalyzer::kFftSize;

    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < N; ++i) {
        size_t bit = N >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    for (size_t len = 2; len <= N; len <<= 1) {
        const float ang = static_cast<float>(-2.0 * kPi / static_cast<double>(len));
        const std::complex<float> wlen(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < N; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            const size_t half = len >> 1;
            for (size_t j = 0; j < half; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + half] * w;
                a[i + j] = u + v;
                a[i + j + half] = u - v;
                w *= wlen;
            }
        }
    }
}
} // namespace

FftAnalyzer::FftAnalyzer() {
    for (size_t i = 0; i < kFftSize; ++i) {
        hannWindow_[i] = static_cast<float>(
            0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(kFftSize - 1))));
    }
}

void FftAnalyzer::reset() noexcept {
    rtAccum_.fill(0.0f);
    rtAccumPos_ = 0;
    smoothedBars_.fill(0.0f);
    peakHold_.fill(0.0f);
    peakVelocity_.fill(0.0f);
}

void FftAnalyzer::pushInterleaved(const float* interleaved, size_t frameCount, uint32_t channels) noexcept {
    if (!enabled_.load(std::memory_order_acquire) || frameCount == 0 || channels == 0) {
        return;
    }

    const float invCh = 1.0f / static_cast<float>(channels);
    for (size_t f = 0; f < frameCount; ++f) {
        float mono = 0.0f;
        const float* ptr = interleaved + f * channels;
        for (uint32_t c = 0; c < channels; ++c) {
            mono += ptr[c];
        }
        rtAccum_[rtAccumPos_++] = mono * invCh;

        if (rtAccumPos_ >= kFftSize) {
            const int writeIdx = 1 - readyIdx_.load(std::memory_order_relaxed);
            snapshots_[writeIdx] = rtAccum_;
            readyIdx_.store(writeIdx, std::memory_order_release);
            snapshotSeq_.fetch_add(1, std::memory_order_release);
            // 50% overlap keeps visualizer responsive even at 44.1 kHz
            constexpr size_t half = kFftSize / 2;
            std::copy(rtAccum_.begin() + half, rtAccum_.end(), rtAccum_.begin());
            rtAccumPos_ = half;
        }
    }
}

void FftAnalyzer::computeMagnitudeSpectrum(const std::array<float, kFftSize>& timeSamples,
                                           std::array<float, kFftSize / 2>& outMagnitudes) {
    std::array<std::complex<float>, kFftSize> fftBuf{};
    for (size_t i = 0; i < kFftSize; ++i) {
        const float w = static_cast<float>(
            0.5 * (1.0 - std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(kFftSize - 1))));
        fftBuf[i] = std::complex<float>(timeSamples[i] * w, 0.0f);
    }
    fftRadix2(fftBuf);

    const float norm = 2.0f / static_cast<float>(kFftSize);
    for (size_t i = 0; i < kFftSize / 2; ++i) {
        outMagnitudes[i] = std::abs(fftBuf[i]) * norm;
    }
}

void FftAnalyzer::computeBands(std::array<float, kBandCount>& outBars,
                               std::array<float, kBandCount>& outPeaks,
                               bool isPlaying) {
    if (!isPlaying) {
        for (size_t b = 0; b < kBandCount; ++b) {
            smoothedBars_[b] *= 0.82f;
            if (smoothedBars_[b] < 0.002f) smoothedBars_[b] = 0.0f;
            peakHold_[b] = std::max(0.0f, peakHold_[b] - 0.025f);
        }
        outBars = smoothedBars_;
        outPeaks = peakHold_;
        return;
    }

    const uint64_t seq = snapshotSeq_.load(std::memory_order_acquire);
    if (seq != lastConsumedSeq_) {
        lastConsumedSeq_ = seq;
        const int idx = readyIdx_.load(std::memory_order_acquire);
        const auto& samples = snapshots_[idx];

        std::array<std::complex<float>, kFftSize> fftBuf{};
        for (size_t i = 0; i < kFftSize; ++i) {
            fftBuf[i] = std::complex<float>(samples[i] * hannWindow_[i], 0.0f);
        }
        fftRadix2(fftBuf);

        const double sr = static_cast<double>(sampleRate_.load(std::memory_order_acquire));
        const double binHz = sr / static_cast<double>(kFftSize);
        const double minFreq = 24.0;
        const double maxFreq = std::min(18000.0, sr * 0.46);
        const double logRatio = std::log(maxFreq / minFreq);
        const float norm = 4.0f / static_cast<float>(kFftSize); // Hann coherent gain compensation

        for (size_t b = 0; b < kBandCount; ++b) {
            const double f0 = minFreq * std::exp(logRatio * static_cast<double>(b) / static_cast<double>(kBandCount));
            const double f1 = minFreq * std::exp(logRatio * static_cast<double>(b + 1) / static_cast<double>(kBandCount));
            const size_t binStart = std::clamp<size_t>(
                static_cast<size_t>(std::floor(f0 / binHz)), 1, kFftSize / 2 - 1);
            const size_t binEnd = std::clamp<size_t>(
                std::max<size_t>(binStart + 1, static_cast<size_t>(std::ceil(f1 / binHz))),
                binStart + 1, kFftSize / 2);

            float peakMag = 0.0f;
            for (size_t bin = binStart; bin < binEnd; ++bin) {
                const float mag = std::abs(fftBuf[bin]) * norm;
                if (mag > peakMag) peakMag = mag;
            }

            // Convert magnitude to perceptually pleasing dB scale [-68 dB, -2 dB] -> [0, 1]
            const float db = 20.0f * std::log10(std::max(1e-6f, peakMag));
            const float target = std::clamp((db + 68.0f) / 66.0f, 0.0f, 1.0f);

            // Fast attack, smooth exponential decay
            if (target > smoothedBars_[b]) {
                smoothedBars_[b] = smoothedBars_[b] * 0.35f + target * 0.65f;
            } else {
                smoothedBars_[b] = smoothedBars_[b] * 0.78f + target * 0.22f;
            }

            // Gravity peak-hold
            if (smoothedBars_[b] >= peakHold_[b]) {
                peakHold_[b] = smoothedBars_[b];
                peakVelocity_[b] = 0.0f;
            } else {
                peakVelocity_[b] += 0.0022f;
                peakHold_[b] = std::max(smoothedBars_[b], peakHold_[b] - peakVelocity_[b]);
            }
        }
    } else {
        // Gentle decay between snapshots
        for (size_t b = 0; b < kBandCount; ++b) {
            smoothedBars_[b] *= 0.94f;
            peakHold_[b] = std::max(smoothedBars_[b], peakHold_[b] - 0.008f);
        }
    }

    outBars = smoothedBars_;
    outPeaks = peakHold_;
}

} // namespace sonora
