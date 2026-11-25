#include "audio/FFmpegDecoder.h"
#include "audio/IAudioOutput.h"
#include "audio/PlaybackEngine.h"

#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <cmath>
#include <vector>

using namespace sonora;

namespace {

class MockAudioOutput final : public IAudioOutput {
public:
    QVector<AudioDeviceInfo> enumerateDevices() const override {
        return {{QStringLiteral("mock-0"), QStringLiteral("Mock DAC"), true}};
    }

    bool open(const OutputConfig& config,
              int requestedSampleRate,
              int requestedChannels,
              AudioRenderCallback callback,
              ActiveOutputStatus* statusOut) override {
        cb_ = std::move(callback);
        status_.sampleRate = requestedSampleRate > 0 ? requestedSampleRate : 48000;
        status_.channels = requestedChannels > 0 ? requestedChannels : 2;
        status_.hardwareFormat = QStringLiteral("32-bit Float");
        status_.exclusiveActive = (config.mode == OutputMode::WasapiExclusive);
        status_.bitPerfectActive = status_.exclusiveActive && config.bitPerfectBypass;
        status_.deviceName = QStringLiteral("Mock DAC");
        open_ = true;
        if (statusOut) *statusOut = status_;
        return true;
    }

    bool start() override { started_ = open_; return started_; }
    void stop() override { started_ = false; }
    void close() override { started_ = false; open_ = false; cb_ = nullptr; }
    bool isOpen() const override { return open_; }
    bool isStarted() const override { return started_; }
    ActiveOutputStatus activeStatus() const override { return status_; }

    void pull(float* out, uint32_t frames) {
        if (cb_) cb_(out, frames, static_cast<uint32_t>(status_.channels));
    }

private:
    AudioRenderCallback cb_;
    ActiveOutputStatus status_;
    bool open_ = false;
    bool started_ = false;
};

bool writeSyntheticWav(const QString& wavPath, int freqHz, int sampleRate, double durationSec) {
    const uint32_t channels = 2;
    const uint32_t totalFrames = static_cast<uint32_t>(std::llround(sampleRate * durationSec));
    const uint32_t dataBytes = totalFrames * channels * sizeof(int16_t);
    const uint32_t riffSize = 36 + dataBytes;

    QFile f(wavPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    auto writeU32 = [&](uint32_t v) {
        char b[4] = {char(v & 0xFF), char((v >> 8) & 0xFF), char((v >> 16) & 0xFF), char((v >> 24) & 0xFF)};
        f.write(b, 4);
    };
    auto writeU16 = [&](uint16_t v) {
        char b[2] = {char(v & 0xFF), char((v >> 8) & 0xFF)};
        f.write(b, 2);
    };

    f.write("RIFF", 4);
    writeU32(riffSize);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    writeU32(16);
    writeU16(1); // PCM
    writeU16(channels);
    writeU32(static_cast<uint32_t>(sampleRate));
    writeU32(static_cast<uint32_t>(sampleRate) * channels * 2);
    writeU16(channels * 2);
    writeU16(16);
    f.write("data", 4);
    writeU32(dataBytes);

    std::vector<int16_t> samples(totalFrames * channels);
    constexpr double kTwoPi = 6.28318530717958647692;
    for (uint32_t i = 0; i < totalFrames; ++i) {
        const double s = 0.5 * std::sin(kTwoPi * freqHz * static_cast<double>(i) / static_cast<double>(sampleRate));
        const int16_t pcm = static_cast<int16_t>(std::lround(s * 32767.0));
        samples[i * 2 + 0] = pcm;
        samples[i * 2 + 1] = pcm;
    }
    return f.write(reinterpret_cast<const char*>(samples.data()), dataBytes) == static_cast<qint64>(dataBytes);
}

bool generateSyntheticAudio(const QString& outPath,
                            int freqHz,
                            int sampleRate,
                            double durationSec,
                            const QStringList& extraArgs = {}) {
    if (outPath.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive) && extraArgs.size() <= 2) {
        return writeSyntheticWav(outPath, freqHz, sampleRate, durationSec);
    }

    const QString tmpWav = outPath + QStringLiteral(".src.wav");
    if (!writeSyntheticWav(tmpWav, freqHz, sampleRate, durationSec)) {
        return false;
    }

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LD_LIBRARY_PATH"),
               QStringLiteral("/home/antigravity/deps/ffmpeg-8.1/lib:") + env.value(QStringLiteral("LD_LIBRARY_PATH")));
    proc.setProcessEnvironment(env);

    QStringList args = {
        QStringLiteral("-y"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), tmpWav
    };
    args.append(extraArgs);
    args.append(outPath);

    proc.start(QStringLiteral("/home/antigravity/deps/ffmpeg-8.1/bin/ffmpeg"), args);
    const bool ok = proc.waitForFinished(15000) && proc.exitCode() == 0;
    QFile::remove(tmpWav);
    return ok;
}

} // namespace

class TestFFmpegDecoder : public QObject {
    Q_OBJECT

private slots:
    void probeAndDecodeFlacWithReplayGain() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString flacPath = dir.filePath(QStringLiteral("hi_res_test.flac"));

        QVERIFY(generateSyntheticAudio(
            flacPath, 1000, 96000, 1.0,
            {
                QStringLiteral("-c:a"), QStringLiteral("flac"),
                QStringLiteral("-sample_fmt"), QStringLiteral("s32"),
                QStringLiteral("-metadata"), QStringLiteral("title=Nocturne"),
                QStringLiteral("-metadata"), QStringLiteral("artist=Sonora Lab"),
                QStringLiteral("-metadata"), QStringLiteral("album=HiRes Suite"),
                QStringLiteral("-metadata"), QStringLiteral("REPLAYGAIN_TRACK_GAIN=-6.25 dB"),
                QStringLiteral("-metadata"), QStringLiteral("REPLAYGAIN_TRACK_PEAK=0.950000"),
                QStringLiteral("-metadata"), QStringLiteral("LYRICS=[00:00.50]Hello Sonora")
            }));

        TrackInfo probed;
        QString err;
        QVERIFY2(FFmpegDecoder::probeFile(flacPath, probed, &err), qPrintable(err));
        QCOMPARE(probed.title, QStringLiteral("Nocturne"));
        QCOMPARE(probed.artist, QStringLiteral("Sonora Lab"));
        QCOMPARE(probed.codec, QStringLiteral("FLAC"));
        QCOMPARE(probed.sampleRate, 96000);
        QCOMPARE(probed.formatBadge, QStringLiteral("Hi-Res"));
        QVERIFY(probed.hasRgTrack);
        QVERIFY(std::abs(probed.rgTrackGainDb - (-6.25)) < 0.05);
        QCOMPARE(probed.embeddedLyrics, QStringLiteral("[00:00.50]Hello Sonora"));

        // Stream-decode and verify non-silent energy & frame count
        FFmpegDecoder dec;
        QVERIFY2(dec.open(flacPath, 96000, 2, 176400, 0, 0, &err), qPrintable(err));
        std::vector<float> buf(4096 * 2);
        size_t totalFrames = 0;
        double energy = 0.0;
        bool eof = false;
        while (!eof) {
            const size_t n = dec.decodeFrames(buf.data(), 4096, eof);
            for (size_t i = 0; i < n * 2; ++i) energy += buf[i] * buf[i];
            totalFrames += n;
        }
        QVERIFY(totalFrames >= 94000 && totalFrames <= 98000);
        QVERIFY(energy > 1000.0);
    }

    void cueVirtualSubtrackAndContiguousWindow() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString wavPath = dir.filePath(QStringLiteral("live_album.wav"));
        QVERIFY(generateSyntheticAudio(wavPath, 440, 44100, 3.0, {QStringLiteral("-c:a"), QStringLiteral("pcm_s16le")}));

        FFmpegDecoder dec;
        QString err;
        // Slice subtrack 1: [500ms, 1500ms] => ~44100 frames
        QVERIFY2(dec.open(wavPath, 44100, 2, 176400, 500, 1500, &err), qPrintable(err));
        std::vector<float> buf(2048 * 2);
        size_t sub1Frames = 0;
        bool eof = false;
        while (!eof) {
            sub1Frames += dec.decodeFrames(buf.data(), 2048, eof);
        }
        QVERIFY(sub1Frames >= 43500 && sub1Frames <= 44700);

        // Advance seamlessly to contiguous subtrack 2: [1500ms, 2500ms] without reopening
        dec.advanceContiguousCueWindow(1500, 2500);
        size_t sub2Frames = 0;
        eof = false;
        while (!eof) {
            sub2Frames += dec.decodeFrames(buf.data(), 2048, eof);
        }
        QVERIFY(sub2Frames >= 43500 && sub2Frames <= 44700);
    }

    void gaplessPlaybackEngineTransition() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString t1Path = dir.filePath(QStringLiteral("track1.wav"));
        const QString t2Path = dir.filePath(QStringLiteral("track2.wav"));
        QVERIFY(generateSyntheticAudio(t1Path, 440, 44100, 0.35, {QStringLiteral("-c:a"), QStringLiteral("pcm_s16le")}));
        QVERIFY(generateSyntheticAudio(t2Path, 880, 44100, 0.35, {QStringLiteral("-c:a"), QStringLiteral("pcm_s16le")}));

        auto mockRaw = std::make_unique<MockAudioOutput>();
        MockAudioOutput* mockPtr = mockRaw.get();
        PlaybackEngine engine(std::move(mockRaw));
        engine.setVolume(1.0);

        TrackInfo a, b;
        QVERIFY(FFmpegDecoder::probeFile(t1Path, a));
        QVERIFY(FFmpegDecoder::probeFile(t2Path, b));
        a.id = 1;
        b.id = 2;

        QSignalSpy trackSpy(&engine, &PlaybackEngine::currentTrackChanged);
        engine.setQueue({a, b}, 0, true);

        // Wait for track 1 to begin playing
        QTRY_VERIFY_WITH_TIMEOUT(trackSpy.count() >= 1 && mockPtr->isStarted(), 3000);
        QCOMPARE(engine.currentIndex(), 0);

        // Pull audio blocks deterministically to consume track 1 + cross gapless boundary into track 2
        std::vector<float> sink(1024 * 2, 0.0f);
        for (int iter = 0; iter < 55 && engine.currentIndex() == 0; ++iter) {
            mockPtr->pull(sink.data(), 1024);
            QCoreApplication::processEvents();
            QTest::qWait(10);
        }

        QTRY_COMPARE_WITH_TIMEOUT(engine.currentIndex(), 1, 3000);
    }
};

QTEST_MAIN(TestFFmpegDecoder)
#include "test_ffmpeg_decoder.moc"
