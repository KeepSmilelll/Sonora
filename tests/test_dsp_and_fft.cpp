#include "audio/dsp/BiquadEq.h"
#include "audio/dsp/FftAnalyzer.h"
#include "audio/dsp/ReplayGainProcessor.h"

#include <QtTest/QtTest>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

using namespace virela;

namespace {
constexpr double kTwoPi = 6.28318530717958647692;

double measureSineRmsAfterEq(BiquadEq& eq, double freqHz, double sampleRate) {
    eq.resetState();
    constexpr size_t kFrames = 16384;
    std::vector<float> stereo(kFrames * 2);
    for (size_t i = 0; i < kFrames; ++i) {
        const float s = static_cast<float>(0.25 * std::sin(kTwoPi * freqHz * static_cast<double>(i) / sampleRate));
        stereo[i * 2 + 0] = s;
        stereo[i * 2 + 1] = s;
    }
    eq.processInterleaved(stereo.data(), kFrames, 2);

    // Measure RMS on the steady-state second half
    double sumSq = 0.0;
    const size_t start = kFrames / 2;
    for (size_t i = start; i < kFrames; ++i) {
        const double v = stereo[i * 2];
        sumSq += v * v;
    }
    return std::sqrt(sumSq / static_cast<double>(kFrames - start));
}
} // namespace

class TestDspAndFft : public QObject {
    Q_OBJECT

private slots:
    void biquadEqBoostAndCutAccuracy() {
        BiquadEq eq;
        eq.setSampleRate(48000.0);
        eq.setEnabled(true);

        // Flat baseline RMS for 0.25-amplitude sine is 0.25 / sqrt(2) ~= 0.17678
        const double baseRms = measureSineRmsAfterEq(eq, 1000.0, 48000.0);

        // Band 5 is 1000 Hz: boost +6.0 dB (~2x amplitude)
        eq.setBandGainDb(5, 6.0);
        const double boostRms = measureSineRmsAfterEq(eq, 1000.0, 48000.0);
        const double measuredBoostDb = 20.0 * std::log10(boostRms / baseRms);
        QVERIFY2(std::abs(measuredBoostDb - 6.0) < 0.25,
                 qPrintable(QStringLiteral("Measured boost %1 dB != 6.0 dB").arg(measuredBoostDb)));
        QVERIFY(std::abs(eq.magnitudeDbAt(1000.0) - 6.0) < 0.15);

        // Cut -6.0 dB (~0.5x amplitude)
        eq.setBandGainDb(5, -6.0);
        const double cutRms = measureSineRmsAfterEq(eq, 1000.0, 48000.0);
        const double measuredCutDb = 20.0 * std::log10(cutRms / baseRms);
        QVERIFY2(std::abs(measuredCutDb - (-6.0)) < 0.25,
                 qPrintable(QStringLiteral("Measured cut %1 dB != -6.0 dB").arg(measuredCutDb)));
    }

    void replayGainAndSoftLimiter() {
        TrackInfo t;
        t.hasRgTrack = true;
        t.rgTrackGainDb = -6.0206; // ~= 0.5x linear
        t.rgTrackPeak = 0.8;
        t.hasRgAlbum = true;
        t.rgAlbumGainDb = 6.0;     // ~= 2.0x linear, would clip against peak 0.8!
        t.rgAlbumPeak = 0.8;

        QCOMPARE(ReplayGainProcessor::computeLinearGain(t, ReplayGainMode::Off), 1.0);

        const double trackLin = ReplayGainProcessor::computeLinearGain(t, ReplayGainMode::Track);
        QVERIFY(std::abs(trackLin - 0.5) < 0.01);

        // Anti-clipping peak safeguard caps album gain so linear * 0.8 <= 0.999
        const double albumLin = ReplayGainProcessor::computeLinearGain(t, ReplayGainMode::Album, 0.0, 0.0, true);
        QVERIFY(albumLin * t.rgAlbumPeak <= 1.0);

        // Soft limiter keeps signals <= 0.95 untouched and smoothly compresses > 1.0 below 1.0
        QCOMPARE(ReplayGainProcessor::softLimit(0.5f), 0.5f);
        QCOMPARE(ReplayGainProcessor::softLimit(-0.9f), -0.9f);
        QVERIFY(ReplayGainProcessor::softLimit(2.5f) < 1.0f);
        QVERIFY(ReplayGainProcessor::softLimit(-2.5f) > -1.0f);
    }

    void fftAnalyzerPeakBinAndBandEnergy() {
        constexpr double kSr = 48000.0;
        constexpr double kToneHz = 1000.0;

        std::array<float, FftAnalyzer::kFftSize> sine{};
        for (size_t i = 0; i < sine.size(); ++i) {
            sine[i] = static_cast<float>(std::sin(kTwoPi * kToneHz * static_cast<double>(i) / kSr));
        }

        std::array<float, FftAnalyzer::kFftSize / 2> mags{};
        FftAnalyzer::computeMagnitudeSpectrum(sine, mags);

        const auto maxIt = std::max_element(mags.begin(), mags.end());
        const size_t peakBin = static_cast<size_t>(std::distance(mags.begin(), maxIt));
        const size_t expectedBin = static_cast<size_t>(std::llround(kToneHz * FftAnalyzer::kFftSize / kSr));
        QVERIFY(peakBin >= expectedBin - 1 && peakBin <= expectedBin + 1);

        // End-to-end lock-free pushInterleaved -> computeBands verification
        FftAnalyzer analyzer;
        analyzer.setSampleRate(48000);
        analyzer.setEnabled(true);

        std::vector<float> stereo(FftAnalyzer::kFftSize * 2);
        for (size_t i = 0; i < FftAnalyzer::kFftSize; ++i) {
            stereo[i * 2 + 0] = sine[i];
            stereo[i * 2 + 1] = sine[i];
        }
        analyzer.pushInterleaved(stereo.data(), FftAnalyzer::kFftSize, 2);

        std::array<float, FftAnalyzer::kBandCount> bars{};
        std::array<float, FftAnalyzer::kBandCount> peaks{};
        analyzer.computeBands(bars, peaks, true);

        const float maxBar = *std::max_element(bars.begin(), bars.end());
        QVERIFY2(maxBar > 0.45f, qPrintable(QStringLiteral("Expected strong 1kHz band, got %1").arg(maxBar)));
        QVERIFY(bars[0] < 0.15f); // Sub-bass band should remain quiet for 1 kHz pure tone
    }
};

QTEST_MAIN(TestDspAndFft)
#include "test_dsp_and_fft.moc"
