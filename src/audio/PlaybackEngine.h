#pragma once

#include "audio/AudioRingBuffer.h"
#include "audio/AudioTypes.h"
#include "audio/FFmpegDecoder.h"
#include "audio/IAudioOutput.h"
#include "audio/dsp/BiquadEq.h"
#include "audio/dsp/FftAnalyzer.h"

#include <QObject>
#include <QTimer>
#include <QVector>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace virela {

class StreamCacheSession;

struct ResolvedOnlinePlayback {
    QString localCachePath;
    std::shared_ptr<StreamCacheSession> session;
    QString directUrl;
    QString formatBadge;
    QString codec;
    int bitrateKbps = 0;
    QString error;
};

class PlaybackEngine : public QObject {
    Q_OBJECT

public:
    explicit PlaybackEngine(std::unique_ptr<IAudioOutput> output = nullptr, QObject* parent = nullptr);
    ~PlaybackEngine() override;

    // Queue & transport control (called from UI/control thread)
    void setQueue(const QVector<TrackInfo>& tracks, int startIndex = 0, bool autoPlay = true);
    void appendToQueue(const QVector<TrackInfo>& tracks);
    void insertNext(const TrackInfo& track);
    void removeFromQueue(int index);
    void clearQueue();

    void playTrack(const TrackInfo& track);
    void playIndex(int index);
    void play();
    void pause();
    void togglePlayPause();
    void stop();
    void next();
    void previous();
    void seekMs(int64_t positionMs);

    // State & configuration accessors
    PlaybackState state() const noexcept { return state_.load(std::memory_order_acquire); }
    int64_t positionMs() const noexcept;
    int64_t durationMs() const noexcept;
    double bufferedRatio() const noexcept { return bufferedRatio_.load(std::memory_order_acquire); }
    TrackInfo currentTrack() const;
    int currentIndex() const;
    QVector<TrackInfo> queue() const;

    void setVolume(double linear01);
    double volume() const noexcept { return targetVolume_.load(std::memory_order_acquire); }
    void setMuted(bool muted);
    bool isMuted() const noexcept { return muted_.load(std::memory_order_acquire); }

    void setRepeatMode(RepeatMode mode);
    RepeatMode repeatMode() const noexcept { return repeatMode_.load(std::memory_order_acquire); }
    void setShuffle(bool enabled);
    bool isShuffle() const noexcept { return shuffle_.load(std::memory_order_acquire); }

    // Audio output & DSP configuration
    QVector<AudioDeviceInfo> enumerateDevices() const;
    void setOutputConfig(const OutputConfig& config);
    OutputConfig outputConfig() const;
    ActiveOutputStatus activeOutputStatus() const;

    void setReplayGainMode(ReplayGainMode mode);
    ReplayGainMode replayGainMode() const noexcept { return replayGainMode_.load(std::memory_order_acquire); }
    void setReplayGainPreampDb(double db);
    double replayGainPreampDb() const noexcept { return rgPreampDb_.load(std::memory_order_acquire); }

    BiquadEq& equalizer() noexcept { return eq_; }
    const BiquadEq& equalizer() const noexcept { return eq_; }
    FftAnalyzer& fftAnalyzer() noexcept { return fft_; }
    const FftAnalyzer& fftAnalyzer() const noexcept { return fft_; }

    // Direct RT pull helper exposed for deterministic unit tests (with NullAudioOutput)
    void renderFramesForTest(float* interleavedOut, uint32_t frameCount, uint32_t channels);

    using OnlineStreamResolverFn = std::function<ResolvedOnlinePlayback(const TrackInfo&, const std::atomic<bool>*)>;
    void setOnlineStreamResolver(OnlineStreamResolverFn fn);

signals:
    void stateChanged(virela::PlaybackState state);
    void currentTrackChanged(const virela::TrackInfo& track, int queueIndex);
    void queueChanged();
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void bufferedRatioChanged(double ratio);
    void outputStatusChanged(const virela::ActiveOutputStatus& status);
    void errorOccurred(const QString& message);
    void trackPlaybackStarted(qint64 trackId);
    void trackStartedPlayback(const virela::TrackInfo& track);

private:
    enum class DecodeCommandType {
        None,
        OpenTrack,
        Seek,
        Stop,
        ReconfigureOutput,
        Shutdown
    };

    struct DecodeCommand {
        DecodeCommandType type = DecodeCommandType::None;
        int queueIndex = -1;
        TrackInfo track;
        int64_t seekMs = 0;
    };

    void startDecodeThread();
    void stopDecodeThread();
    void decodeLoop();

    void handleOpenTrackLocked(const TrackInfo& track, int queueIndex);
    bool openOutputForFormat(int sampleRate, int channels);
    void renderAudioCallback(float* interleavedOut, uint32_t frameCount, uint32_t channels) noexcept;
    void rebuildShuffleOrderLocked(int anchorIndex);
    int computeNextIndexLocked(bool userTriggered, int fromIndex = -2) const;
    int computePrevIndexLocked() const;
    void onUiTick();
    void handlePendingTrackTransition();
    void handlePendingDeviceReconfigure();

    std::unique_ptr<IAudioOutput> output_;
    AudioRingBuffer ringBuffer_{131072, 2}; // ~1.36s @ 96kHz / ~2.73s @ 48kHz (~1 MB)
    LockFreeMarkerQueue<16> boundaryQueue_;

    BiquadEq eq_;
    FftAnalyzer fft_;

    mutable std::mutex queueMutex_;
    QVector<TrackInfo> queue_;
    QVector<int> shuffleOrder_;
    int currentQueueIndex_ = -1;
    TrackInfo currentTrack_;
    OutputConfig outputConfig_;
    ActiveOutputStatus activeOutputStatus_;

    // Decoder thread state
    std::thread decodeThread_;
    std::mutex cmdMutex_;
    std::condition_variable cmdCv_;
    DecodeCommand pendingCmd_;
    bool hasPendingCmd_ = false;
    std::atomic<bool> decodeRunning_{false};
    std::atomic<bool> abortDecode_{false};

    // Real-time atomic playback parameters
    std::atomic<PlaybackState> state_{PlaybackState::Stopped};
    std::atomic<RepeatMode> repeatMode_{RepeatMode::Off};
    std::atomic<bool> shuffle_{false};
    std::atomic<ReplayGainMode> replayGainMode_{ReplayGainMode::Off};
    std::atomic<double> rgPreampDb_{0.0};
    std::atomic<double> targetVolume_{0.85};
    std::atomic<bool> muted_{false};

    // RT-owned smooth ramp & clock variables
    float rtCurrentGain_ = 0.85f;
    float rtFadeGain_ = 1.0f;
    float rtRgGain_ = 1.0f;
    int rtFadeSamplesRemaining_ = 0;
    bool rtBitPerfectEligible_ = false;

    std::atomic<int> activeSampleRate_{48000};
    std::atomic<int> activeChannels_{2};
    std::atomic<int64_t> activeDurationMs_{0};
    std::atomic<int64_t> trackBaseMs_{0};
    std::atomic<uint64_t> trackStartAbsFrame_{0};
    std::atomic<uint64_t> playedAbsFrames_{0};
    std::atomic<bool> rtFlushRequested_{false};
    std::atomic<bool> rtPaused_{true};

    // Transition signals from RT thread -> UI thread (lock-free)
    std::atomic<bool> pendingTransitionFlag_{false};
    std::atomic<int> pendingTransitionQueueIndex_{-1};
    std::atomic<bool> pendingEosFlag_{false};
    std::atomic<bool> pendingReconfigureFlag_{false};
    std::atomic<int> pendingReconfigureIndex_{-1};

    QTimer uiTickTimer_;
    int64_t lastEmittedPosMs_ = -1;
    double lastEmittedBufferedRatio_ = 0.0;

    mutable std::mutex streamSessionMutex_;
    std::shared_ptr<StreamCacheSession> activeStreamSession_;
    std::atomic<double> bufferedRatio_{0.0};

    mutable std::mutex resolverMutex_;
    OnlineStreamResolverFn onlineStreamResolver_;
};

} // namespace virela
