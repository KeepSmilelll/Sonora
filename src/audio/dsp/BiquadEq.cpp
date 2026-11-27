#include "audio/dsp/BiquadEq.h"

#include <algorithm>
#include <cmath>
#include <complex>

namespace virela {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

BiquadEq::BiquadEq() {
    rebuildAndPublish();
    resetState();
}

void BiquadEq::setEnabled(bool enabled) {
    enabled_.store(enabled, std::memory_order_release);
}

void BiquadEq::setSampleRate(double sampleRate) {
    sampleRate_ = std::max(8000.0, sampleRate);
    rebuildAndPublish();
}

void BiquadEq::setPreampDb(double preampDb) {
    preampDb_ = std::clamp(preampDb, -15.0, 15.0);
    rebuildAndPublish();
}

void BiquadEq::setBandGainDb(size_t bandIndex, double gainDb) {
    if (bandIndex >= kBandCount) return;
    gainsDb_[bandIndex] = std::clamp(gainDb, -12.0, 12.0);
    rebuildAndPublish();
}

void BiquadEq::setAllBandGainsDb(const std::array<double, kBandCount>& gainsDb) {
    for (size_t i = 0; i < kBandCount; ++i) {
        gainsDb_[i] = std::clamp(gainsDb[i], -12.0, 12.0);
    }
    rebuildAndPublish();
}

BiquadEq::Coeffs BiquadEq::designPeakingFilter(double freqHz, double sampleRate, double q, double gainDb) {
    if (std::abs(gainDb) < 1e-4 || freqHz >= sampleRate * 0.49) {
        return Coeffs{};
    }
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * freqHz / sampleRate;
    const double cosW0 = std::cos(w0);
    const double alpha = std::sin(w0) / (2.0 * q);

    const double b0 = 1.0 + alpha * A;
    const double b1 = -2.0 * cosW0;
    const double b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A;
    const double a1 = -2.0 * cosW0;
    const double a2 = 1.0 - alpha / A;

    return Coeffs{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

BiquadEq::Coeffs BiquadEq::designLowShelf(double freqHz, double sampleRate, double slope, double gainDb) {
    if (std::abs(gainDb) < 1e-4 || freqHz >= sampleRate * 0.49) {
        return Coeffs{};
    }
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * freqHz / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = (sinW0 / 2.0) * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
    const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

    const double b0 = A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha);
    const double b1 = 2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0);
    const double b2 = A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
    const double a0 = (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
    const double a1 = -2.0 * ((A - 1.0) + (A + 1.0) * cosW0);
    const double a2 = (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha;

    return Coeffs{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

BiquadEq::Coeffs BiquadEq::designHighShelf(double freqHz, double sampleRate, double slope, double gainDb) {
    if (std::abs(gainDb) < 1e-4 || freqHz >= sampleRate * 0.49) {
        return Coeffs{};
    }
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2.0 * kPi * freqHz / sampleRate;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = (sinW0 / 2.0) * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
    const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

    const double b0 = A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha);
    const double b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0);
    const double b2 = A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
    const double a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
    const double a1 = 2.0 * ((A - 1.0) - (A + 1.0) * cosW0);
    const double a2 = (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha;

    return Coeffs{b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0};
}

void BiquadEq::rebuildAndPublish() {
    const int cur = activeIdx_.load(std::memory_order_relaxed);
    const int next = 1 - cur;
    ParamSet& dst = buffers_[next];

    dst.preampLinear = std::pow(10.0, preampDb_ / 20.0);
    for (size_t i = 0; i < kBandCount; ++i) {
        if (i == 0) {
            dst.bands[i] = designLowShelf(kFrequencies[i], sampleRate_, 0.9, gainsDb_[i]);
        } else if (i == kBandCount - 1) {
            dst.bands[i] = designHighShelf(kFrequencies[i], sampleRate_, 0.9, gainsDb_[i]);
        } else {
            // 1-octave Q ~= 1.414 yields smooth overlapping graphic/parametric bands
            dst.bands[i] = designPeakingFilter(kFrequencies[i], sampleRate_, 1.41421356, gainsDb_[i]);
        }
    }

    activeIdx_.store(next, std::memory_order_release);
}

double BiquadEq::magnitudeDbAt(double fHz) const {
    const ParamSet& params = buffers_[activeIdx_.load(std::memory_order_acquire)];
    const double w = 2.0 * kPi * std::clamp(fHz, 10.0, sampleRate_ * 0.495) / sampleRate_;
    const std::complex<double> z1 = std::polar(1.0, -w);
    const std::complex<double> z2 = std::polar(1.0, -2.0 * w);

    double totalMag = params.preampLinear;
    for (size_t i = 0; i < kBandCount; ++i) {
        const auto& c = params.bands[i];
        const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
        const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
        totalMag *= std::abs(num / den);
    }
    return 20.0 * std::log10(std::max(1e-9, totalMag));
}

void BiquadEq::resetState() noexcept {
    for (auto& band : state_) {
        for (auto& ch : band) {
            ch.z1 = 0.0;
            ch.z2 = 0.0;
        }
    }
}

void BiquadEq::processInterleaved(float* interleaved, size_t frameCount, uint32_t channels) noexcept {
    if (!enabled_.load(std::memory_order_acquire) || frameCount == 0 || channels == 0) {
        return;
    }

    const int idx = activeIdx_.load(std::memory_order_acquire);
    rtLastIdx_ = idx;
    const ParamSet& params = buffers_[idx];
    const uint32_t chCount = std::min<uint32_t>(channels, static_cast<uint32_t>(kMaxChannels));

    for (size_t f = 0; f < frameCount; ++f) {
        float* framePtr = interleaved + f * channels;
        for (uint32_t ch = 0; ch < chCount; ++ch) {
            double x = static_cast<double>(framePtr[ch]) * params.preampLinear;
            for (size_t b = 0; b < kBandCount; ++b) {
                const Coeffs& c = params.bands[b];
                FilterState& st = state_[b][ch];
                const double y = c.b0 * x + st.z1;
                st.z1 = c.b1 * x - c.a1 * y + st.z2;
                st.z2 = c.b2 * x - c.a2 * y;
                x = y;
            }
            framePtr[ch] = static_cast<float>(x);
        }
    }
}

const std::array<BiquadEq::Preset, 7>& BiquadEq::builtInPresets() {
    static const std::array<Preset, 7> kPresets = {{
        {QStringLiteral("flat"),       QStringLiteral("Flat"),       0.0,  { 0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0,  0.0}},
        {QStringLiteral("vocal"),      QStringLiteral("Vocal"),     -1.0,  {-1.5, -1.0,  0.0,  1.5,  3.0,  3.5,  2.5,  1.0,  0.0, -0.5}},
        {QStringLiteral("bass"),       QStringLiteral("Bass Boost"),-2.5,  { 5.0,  4.5,  3.0,  1.5,  0.0,  0.0,  0.0,  0.5,  1.0,  1.0}},
        {QStringLiteral("classical"),  QStringLiteral("Classical"), -1.0,  { 2.5,  2.0,  1.0,  0.0, -0.5, -0.5,  0.5,  1.5,  2.5,  3.0}},
        {QStringLiteral("acoustic"),   QStringLiteral("Acoustic"),  -1.0,  { 2.0,  1.5,  1.0,  0.5,  1.5,  1.5,  2.0,  2.5,  2.0,  1.5}},
        {QStringLiteral("electronic"), QStringLiteral("Electronic"),-2.0,  { 4.5,  3.5,  1.5,  0.0, -1.0,  0.5,  1.5,  2.5,  3.5,  4.0}},
        {QStringLiteral("late_night"), QStringLiteral("Late Night"),-1.5,  { 2.5,  2.0,  1.0,  0.5,  0.0, -0.5, -0.5,  0.5,  1.5,  2.0}}
    }};
    return kPresets;
}

} // namespace virela
