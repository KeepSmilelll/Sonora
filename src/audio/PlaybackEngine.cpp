#include "audio/PlaybackEngine.h"
#include "audio/MiniAudioOutput.h"
#include "audio/dsp/ReplayGainProcessor.h"
#include "online/StreamCacheManager.h"

#include <QMetaObject>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <numeric>
#include <random>

namespace virela {

namespace {

bool isContiguousCuePair(const TrackInfo& current, const TrackInfo& next) {
    if (!current.isCue || !next.isCue) return false;
    if (current.filePath.isEmpty() || current.filePath != next.filePath) return false;
    if (current.endMs <= 0) return false;
    return std::abs(next.startMs - current.endMs) <= 25;
}

int desiredSampleRateForTrack(const TrackInfo& track, const OutputConfig& cfg) {
    if (track.isDsd) {
        return cfg.dsdTargetSampleRate > 0 ? cfg.dsdTargetSampleRate : 176400;
    }
    if (cfg.mode == OutputMode::WasapiExclusive && cfg.autoNativeSampleRate) {
        return track.sampleRate > 0 ? track.sampleRate : 44100;
    }
    if (cfg.preferredSampleRate > 0) {
        return cfg.preferredSampleRate;
    }
    return track.sampleRate > 0 ? track.sampleRate : 48000;
}

} // namespace

PlaybackEngine::PlaybackEngine(std::unique_ptr<IAudioOutput> output, QObject* parent)
    : QObject(parent)
    , output_(output ? std::move(output) : std::make_unique<MiniAudioOutput>()) {
    uiTickTimer_.setInterval(33); // ~30 Hz UI position & lock-free transition poll (paused when idle)
    connect(&uiTickTimer_, &QTimer::timeout, this, &PlaybackEngine::onUiTick);
    startDecodeThread();
}

PlaybackEngine::~PlaybackEngine() {
    uiTickTimer_.stop();
    stopDecodeThread();
    if (output_) {
        output_->close();
    }
}

void PlaybackEngine::startDecodeThread() {
    decodeRunning_.store(true, std::memory_order_release);
    decodeThread_ = std::thread(&PlaybackEngine::decodeLoop, this);
}

void PlaybackEngine::stopDecodeThread() {
    if (!decodeRunning_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    abortDecode_.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(cmdMutex_);
        pendingCmd_.type = DecodeCommandType::Shutdown;
        hasPendingCmd_ = true;
    }
    cmdCv_.notify_all();
    if (decodeThread_.joinable()) {
        decodeThread_.join();
    }
}

void PlaybackEngine::setQueue(const QVector<TrackInfo>& tracks, int startIndex, bool autoPlay) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_ = tracks;
        if (queue_.isEmpty()) {
            currentQueueIndex_ = -1;
            currentTrack_ = TrackInfo{};
            shuffleOrder_.clear();
        } else {
            currentQueueIndex_ = std::clamp(startIndex, 0, static_cast<int>(queue_.size()) - 1);
            rebuildShuffleOrderLocked(currentQueueIndex_);
        }
    }
    emit queueChanged();

    if (tracks.isEmpty()) {
        stop();
        return;
    }
    if (autoPlay) {
        playIndex(std::clamp(startIndex, 0, static_cast<int>(tracks.size()) - 1));
    }
}

void PlaybackEngine::appendToQueue(const QVector<TrackInfo>& tracks) {
    if (tracks.isEmpty()) return;
    bool wasEmpty = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        wasEmpty = queue_.isEmpty();
        const int oldSize = static_cast<int>(queue_.size());
        queue_.append(tracks);
        if (shuffle_.load(std::memory_order_acquire)) {
            for (int i = oldSize; i < queue_.size(); ++i) {
                shuffleOrder_.push_back(i);
            }
        }
    }
    emit queueChanged();
    if (wasEmpty) {
        playIndex(0);
    }
}

void PlaybackEngine::insertNext(const TrackInfo& track) {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        const int insertPos = (currentQueueIndex_ >= 0 && currentQueueIndex_ < queue_.size())
            ? currentQueueIndex_ + 1
            : static_cast<int>(queue_.size());
        queue_.insert(insertPos, track);
        if (shuffle_.load(std::memory_order_acquire)) {
            rebuildShuffleOrderLocked(currentQueueIndex_);
        }
    }
    emit queueChanged();
}

void PlaybackEngine::removeFromQueue(int index) {
    bool removedCurrent = false;
    int nextIndexToPlay = -1;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (index < 0 || index >= queue_.size()) return;
        queue_.removeAt(index);
        if (queue_.isEmpty()) {
            currentQueueIndex_ = -1;
            removedCurrent = true;
        } else if (index == currentQueueIndex_) {
            removedCurrent = true;
            nextIndexToPlay = std::min(index, static_cast<int>(queue_.size()) - 1);
        } else if (index < currentQueueIndex_) {
            --currentQueueIndex_;
        }
        if (shuffle_.load(std::memory_order_acquire)) {
            rebuildShuffleOrderLocked(currentQueueIndex_);
        }
    }
    emit queueChanged();
    if (removedCurrent) {
        if (nextIndexToPlay >= 0) {
            playIndex(nextIndexToPlay);
        } else {
            stop();
        }
    }
}

void PlaybackEngine::clearQueue() {
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        queue_.clear();
        shuffleOrder_.clear();
        currentQueueIndex_ = -1;
        currentTrack_ = TrackInfo{};
    }
    stop();
    emit queueChanged();
}

void PlaybackEngine::playTrack(const TrackInfo& track) {
    setQueue({track}, 0, true);
}

void PlaybackEngine::playIndex(int index) {
    TrackInfo target;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (index < 0 || index >= queue_.size()) return;
        currentQueueIndex_ = index;
        target = queue_[index];
        currentTrack_ = target;
    }

    abortDecode_.store(true, std::memory_order_release);
    rtPaused_.store(false, std::memory_order_release);
    state_.store(PlaybackState::Buffering, std::memory_order_release);
    emit stateChanged(PlaybackState::Buffering);

    {
        std::lock_guard<std::mutex> lock(cmdMutex_);
        pendingCmd_ = {DecodeCommandType::OpenTrack, index, target, 0};
        hasPendingCmd_ = true;
    }
    cmdCv_.notify_all();
    uiTickTimer_.start();
}

void PlaybackEngine::play() {
    const auto st = state_.load(std::memory_order_acquire);
    if (st == PlaybackState::Playing) return;
    if (st == PlaybackState::Paused && output_ && output_->isOpen()) {
        rtFadeSamplesRemaining_ = std::max(64, activeSampleRate_.load() / 200);
        rtFadeGain_ = 0.0f;
        rtPaused_.store(false, std::memory_order_release);
        output_->start();
        state_.store(PlaybackState::Playing, std::memory_order_release);
        uiTickTimer_.start();
        emit stateChanged(PlaybackState::Playing);
        return;
    }
    int idx = -1;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (!queue_.isEmpty()) {
            idx = currentQueueIndex_ >= 0 ? currentQueueIndex_ : 0;
        }
    }
    if (idx >= 0) {
        playIndex(idx);
    }
}

void PlaybackEngine::pause() {
    if (state_.load(std::memory_order_acquire) != PlaybackState::Playing) return;
    rtPaused_.store(true, std::memory_order_release);
    if (output_ && output_->isOpen()) {
        output_->stop();
    }
    state_.store(PlaybackState::Paused, std::memory_order_release);
    uiTickTimer_.stop();
    emit stateChanged(PlaybackState::Paused);
}

void PlaybackEngine::togglePlayPause() {
    if (state_.load(std::memory_order_acquire) == PlaybackState::Playing) {
        pause();
    } else {
        play();
    }
}

void PlaybackEngine::stop() {
    abortDecode_.store(true, std::memory_order_release);
    rtPaused_.store(true, std::memory_order_release);
    uiTickTimer_.stop();
    {
        std::lock_guard<std::mutex> lock(streamSessionMutex_);
        activeStreamSession_.reset();
    }
    bufferedRatio_.store(0.0, std::memory_order_release);
    if (lastEmittedBufferedRatio_ != 0.0) {
        lastEmittedBufferedRatio_ = 0.0;
        emit bufferedRatioChanged(0.0);
    }
    {
        std::lock_guard<std::mutex> lock(cmdMutex_);
        pendingCmd_.type = DecodeCommandType::Stop;
        hasPendingCmd_ = true;
    }
    cmdCv_.notify_all();
    state_.store(PlaybackState::Stopped, std::memory_order_release);
    trackBaseMs_.store(0, std::memory_order_release);
    lastEmittedPosMs_ = 0;
    emit stateChanged(PlaybackState::Stopped);
    emit positionChanged(0);
}

void PlaybackEngine::next() {
    int nextIdx = -1;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        nextIdx = computeNextIndexLocked(true);
    }
    if (nextIdx >= 0) {
        playIndex(nextIdx);
    } else {
        stop();
    }
}

void PlaybackEngine::previous() {
    // If more than 3 seconds into current track, restart track first (Apple Music convention)
    if (positionMs() > 3000) {
        seekMs(0);
        return;
    }
    int prevIdx = -1;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        prevIdx = computePrevIndexLocked();
    }
    if (prevIdx >= 0) {
        playIndex(prevIdx);
    }
}

void PlaybackEngine::seekMs(int64_t positionMs) {
    const int64_t dur = activeDurationMs_.load(std::memory_order_acquire);
    const int64_t clamped = std::clamp<int64_t>(positionMs, 0, dur > 0 ? dur : std::max<int64_t>(0, positionMs));
    abortDecode_.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(cmdMutex_);
        pendingCmd_.type = DecodeCommandType::Seek;
        pendingCmd_.seekMs = clamped;
        hasPendingCmd_ = true;
    }
    cmdCv_.notify_all();
    trackBaseMs_.store(clamped, std::memory_order_release);
    trackStartAbsFrame_.store(playedAbsFrames_.load(std::memory_order_acquire), std::memory_order_release);
    lastEmittedPosMs_ = clamped;
    emit positionChanged(clamped);
}

int64_t PlaybackEngine::positionMs() const noexcept {
    const int sr = activeSampleRate_.load(std::memory_order_acquire);
    if (sr <= 0) return 0;
    const uint64_t played = playedAbsFrames_.load(std::memory_order_acquire);
    const uint64_t start = trackStartAbsFrame_.load(std::memory_order_acquire);
    const uint64_t delta = played >= start ? (played - start) : 0;
    const int64_t ms = trackBaseMs_.load(std::memory_order_acquire)
        + static_cast<int64_t>((delta * 1000ULL) / static_cast<uint64_t>(sr));
    const int64_t dur = activeDurationMs_.load(std::memory_order_acquire);
    return dur > 0 ? std::min(ms, dur) : ms;
}

int64_t PlaybackEngine::durationMs() const noexcept {
    return activeDurationMs_.load(std::memory_order_acquire);
}

TrackInfo PlaybackEngine::currentTrack() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return currentTrack_;
}

int PlaybackEngine::currentIndex() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return currentQueueIndex_;
}

QVector<TrackInfo> PlaybackEngine::queue() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return queue_;
}

void PlaybackEngine::setVolume(double linear01) {
    targetVolume_.store(std::clamp(linear01, 0.0, 1.0), std::memory_order_release);
}

void PlaybackEngine::setMuted(bool muted) {
    muted_.store(muted, std::memory_order_release);
}

void PlaybackEngine::setRepeatMode(RepeatMode mode) {
    repeatMode_.store(mode, std::memory_order_release);
}

void PlaybackEngine::setShuffle(bool enabled) {
    shuffle_.store(enabled, std::memory_order_release);
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (enabled) {
        rebuildShuffleOrderLocked(currentQueueIndex_);
    } else {
        shuffleOrder_.clear();
    }
}

QVector<AudioDeviceInfo> PlaybackEngine::enumerateDevices() const {
    return output_ ? output_->enumerateDevices() : QVector<AudioDeviceInfo>{};
}

void PlaybackEngine::setOutputConfig(const OutputConfig& config) {
    bool needReopen = false;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        needReopen = (outputConfig_.deviceId != config.deviceId
                      || outputConfig_.mode != config.mode
                      || outputConfig_.autoNativeSampleRate != config.autoNativeSampleRate
                      || outputConfig_.dsdTargetSampleRate != config.dsdTargetSampleRate
                      || outputConfig_.bitPerfectBypass != config.bitPerfectBypass);
        outputConfig_ = config;
    }
    if (needReopen && state_.load(std::memory_order_acquire) != PlaybackState::Stopped) {
        const int64_t curPos = positionMs();
        const int idx = currentIndex();
        if (idx >= 0) {
            abortDecode_.store(true, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lock(cmdMutex_);
                pendingCmd_.type = DecodeCommandType::ReconfigureOutput;
                pendingCmd_.queueIndex = idx;
                pendingCmd_.track = currentTrack();
                pendingCmd_.seekMs = curPos;
                hasPendingCmd_ = true;
            }
            cmdCv_.notify_all();
        }
    }
}

OutputConfig PlaybackEngine::outputConfig() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return outputConfig_;
}

ActiveOutputStatus PlaybackEngine::activeOutputStatus() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return activeOutputStatus_;
}

void PlaybackEngine::setReplayGainMode(ReplayGainMode mode) {
    replayGainMode_.store(mode, std::memory_order_release);
    const TrackInfo t = currentTrack();
    const double lin = ReplayGainProcessor::computeLinearGain(t, mode, rgPreampDb_.load(std::memory_order_acquire));
    rtRgGain_ = static_cast<float>(lin);
}

void PlaybackEngine::setReplayGainPreampDb(double db) {
    rgPreampDb_.store(std::clamp(db, -15.0, 15.0), std::memory_order_release);
    setReplayGainMode(replayGainMode_.load(std::memory_order_acquire));
}

void PlaybackEngine::rebuildShuffleOrderLocked(int anchorIndex) {
    shuffleOrder_.resize(queue_.size());
    std::iota(shuffleOrder_.begin(), shuffleOrder_.end(), 0);
    if (shuffleOrder_.size() <= 1) return;

    std::mt19937 rng(std::random_device{}());
    std::shuffle(shuffleOrder_.begin(), shuffleOrder_.end(), rng);
    if (anchorIndex >= 0 && anchorIndex < queue_.size()) {
        auto it = std::find(shuffleOrder_.begin(), shuffleOrder_.end(), anchorIndex);
        if (it != shuffleOrder_.end()) {
            std::iter_swap(shuffleOrder_.begin(), it);
        }
    }
}

int PlaybackEngine::computeNextIndexLocked(bool userTriggered, int fromIndex) const {
    if (queue_.isEmpty()) return -1;
    const int baseIdx = (fromIndex == -2) ? currentQueueIndex_ : fromIndex;
    const auto rep = repeatMode_.load(std::memory_order_acquire);
    if (!userTriggered && rep == RepeatMode::One) {
        return baseIdx >= 0 ? baseIdx : 0;
    }

    if (shuffle_.load(std::memory_order_acquire) && shuffleOrder_.size() == queue_.size()) {
        const auto pos = shuffleOrder_.indexOf(baseIdx);
        if (pos >= 0 && pos + 1 < shuffleOrder_.size()) {
            return shuffleOrder_[pos + 1];
        }
        return (rep == RepeatMode::All || userTriggered) ? shuffleOrder_.first() : -1;
    }

    if (baseIdx + 1 < queue_.size()) {
        return baseIdx + 1;
    }
    return (rep == RepeatMode::All) ? 0 : -1;
}

int PlaybackEngine::computePrevIndexLocked() const {
    if (queue_.isEmpty()) return -1;
    if (shuffle_.load(std::memory_order_acquire) && shuffleOrder_.size() == queue_.size()) {
        const auto pos = shuffleOrder_.indexOf(currentQueueIndex_);
        if (pos > 0) return shuffleOrder_[pos - 1];
        return shuffleOrder_.last();
    }
    if (currentQueueIndex_ > 0) return currentQueueIndex_ - 1;
    return static_cast<int>(queue_.size()) - 1;
}

bool PlaybackEngine::openOutputForFormat(int sampleRate, int channels) {
    OutputConfig cfg;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        cfg = outputConfig_;
    }

    ActiveOutputStatus st;
    auto cb = [this](float* out, uint32_t frames, uint32_t ch) {
        renderAudioCallback(out, frames, ch);
    };

    if (!output_->open(cfg, sampleRate, channels, std::move(cb), &st)) {
        return false;
    }

    const size_t ringFrames = static_cast<size_t>(std::max(44100, st.sampleRate)) * 2;
    ringBuffer_.resize(ringFrames, static_cast<uint32_t>(st.channels));
    boundaryQueue_.reset();

    activeSampleRate_.store(st.sampleRate, std::memory_order_release);
    activeChannels_.store(st.channels, std::memory_order_release);
    eq_.setSampleRate(static_cast<double>(st.sampleRate));
    eq_.resetState();
    fft_.setSampleRate(st.sampleRate);
    fft_.reset();

    rtBitPerfectEligible_ = st.exclusiveActive && cfg.bitPerfectBypass;

    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        activeOutputStatus_ = st;
    }
    QMetaObject::invokeMethod(this, [this, st]() {
        emit outputStatusChanged(st);
    }, Qt::QueuedConnection);
    return true;
}

void PlaybackEngine::renderFramesForTest(float* interleavedOut, uint32_t frameCount, uint32_t channels) {
    renderAudioCallback(interleavedOut, frameCount, channels);
}

void PlaybackEngine::setOnlineStreamResolver(OnlineStreamResolverFn fn) {
    std::lock_guard<std::mutex> lock(resolverMutex_);
    onlineStreamResolver_ = std::move(fn);
}

void PlaybackEngine::renderAudioCallback(float* interleavedOut, uint32_t frameCount, uint32_t channels) noexcept {
    if (frameCount == 0 || channels == 0) return;

    if (rtFlushRequested_.exchange(false, std::memory_order_acq_rel)) {
        ringBuffer_.clearFromConsumer();
        boundaryQueue_.clearFromConsumer();
        eq_.resetState();
        rtFadeGain_ = 0.0f;
        rtFadeSamplesRemaining_ = std::max(64, activeSampleRate_.load(std::memory_order_relaxed) / 200);
    }

    if (rtPaused_.load(std::memory_order_acquire)) {
        std::memset(interleavedOut, 0, static_cast<size_t>(frameCount) * channels * sizeof(float));
        return;
    }

    const size_t totalSamples = static_cast<size_t>(frameCount) * channels;
    const uint64_t readStartAbs = ringBuffer_.totalReadFrames();

    // Check if a track boundary or EOS marker falls within this callback block
    StreamBoundaryMarker marker{};
    while (boundaryQueue_.peek(marker)) {
        if (marker.absoluteFrame <= readStartAbs) {
            boundaryQueue_.pop(marker);
            if (marker.isEndOfStream) {
                pendingEosFlag_.store(true, std::memory_order_release);
            } else if (marker.needsDeviceReconfigure) {
                pendingReconfigureIndex_.store(marker.queueIndex, std::memory_order_release);
                pendingReconfigureFlag_.store(true, std::memory_order_release);
                std::memset(interleavedOut, 0, totalSamples * sizeof(float));
                return;
            } else {
                trackStartAbsFrame_.store(marker.absoluteFrame, std::memory_order_release);
                trackBaseMs_.store(marker.trackStartOffsetMs, std::memory_order_release);
                rtRgGain_ = static_cast<float>(marker.rgLinearGain);
                pendingTransitionQueueIndex_.store(marker.queueIndex, std::memory_order_release);
                pendingTransitionFlag_.store(true, std::memory_order_release);
            }
        } else {
            break;
        }
    }

    const size_t readFrames = ringBuffer_.read(interleavedOut, frameCount);
    if (readFrames < frameCount) {
        std::memset(interleavedOut + readFrames * channels,
                    0,
                    (frameCount - readFrames) * channels * sizeof(float));
    }

    playedAbsFrames_.store(ringBuffer_.totalReadFrames(), std::memory_order_release);

    if (readFrames == 0) {
        return;
    }

    const bool isMuted = muted_.load(std::memory_order_relaxed);
    const float tgtVol = isMuted ? 0.0f : static_cast<float>(targetVolume_.load(std::memory_order_relaxed));
    const bool eqOn = eq_.isEnabled();
    const bool rgOn = replayGainMode_.load(std::memory_order_relaxed) != ReplayGainMode::Off;

    // Bit-Perfect bypass: skip all gain/EQ/limiter math when WASAPI Exclusive + neutral DSP + 100% volume
    const bool bitPerfectNow = rtBitPerfectEligible_ && !eqOn && !rgOn && !isMuted
        && std::abs(tgtVol - 1.0f) < 1e-4f && rtFadeSamplesRemaining_ == 0;

    if (!bitPerfectNow) {
        // Apply ReplayGain before EQ
        if (rgOn && std::abs(rtRgGain_ - 1.0f) > 1e-5f) {
            const float rg = rtRgGain_;
            const size_t n = readFrames * channels;
            for (size_t i = 0; i < n; ++i) {
                interleavedOut[i] *= rg;
            }
        }

        // Apply 10-band double-precision parametric EQ
        if (eqOn) {
            eq_.processInterleaved(interleavedOut, readFrames, channels);
        }

        // Smooth per-frame volume ramp + micro fade-in + soft-knee anti-clipping limiter
        const float rampCoeff = 1.0f / static_cast<float>(std::max(128, activeSampleRate_.load(std::memory_order_relaxed) / 100));
        for (size_t f = 0; f < readFrames; ++f) {
            rtCurrentGain_ += (tgtVol - rtCurrentGain_) * std::min(1.0f, rampCoeff * 4.0f);
            float gain = rtCurrentGain_;
            if (rtFadeSamplesRemaining_ > 0) {
                rtFadeGain_ = std::min(1.0f, rtFadeGain_ + 0.008f);
                gain *= rtFadeGain_;
                --rtFadeSamplesRemaining_;
            }
            float* ptr = interleavedOut + f * channels;
            for (uint32_t c = 0; c < channels; ++c) {
                ptr[c] = ReplayGainProcessor::softLimit(ptr[c] * gain);
            }
        }
    }

    // Lock-free FFT tap (only active when visualizer is visible)
    fft_.pushInterleaved(interleavedOut, readFrames, channels);
}

void PlaybackEngine::decodeLoop() {
    auto activeDecoder = std::make_unique<FFmpegDecoder>();
    auto nextDecoder = std::make_unique<FFmpegDecoder>();
    int activeQueueIdx = -1;
    int preloadedQueueIdx = -1;
    TrackInfo activeTrack;
    TrackInfo preloadedTrack;
    bool activeEof = false;
    bool eosPushed = false;
    bool waitingReconfigure = false;

    std::vector<float> decodeChunk(4096 * 8, 0.0f);

    auto setStreamSessionAndRatio = [&](std::shared_ptr<StreamCacheSession> sess, double initialRatio) {
        {
            std::lock_guard<std::mutex> lock(streamSessionMutex_);
            activeStreamSession_ = std::move(sess);
        }
        const double clamped = std::clamp(initialRatio, 0.0, 1.0);
        bufferedRatio_.store(clamped, std::memory_order_release);
        QMetaObject::invokeMethod(this, [this, clamped]() {
            if (std::abs(clamped - lastEmittedBufferedRatio_) >= 0.001 || (clamped == 0.0 && lastEmittedBufferedRatio_ != 0.0)) {
                lastEmittedBufferedRatio_ = clamped;
                emit bufferedRatioChanged(clamped);
            }
        }, Qt::QueuedConnection);
    };

    auto openAndStartTrack = [&](const TrackInfo& requestedTrack, int qIdx, int64_t initialSeekMs) {
        activeDecoder->close();
        nextDecoder->close();
        preloadedQueueIdx = -1;
        activeEof = false;
        eosPushed = false;
        waitingReconfigure = false;
        abortDecode_.store(false, std::memory_order_release);
        setStreamSessionAndRatio(nullptr, 0.0);

        OutputConfig cfg;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            cfg = outputConfig_;
        }

        // Probe first if sampleRate unknown (local tracks) or resolve stream first (online tracks)
        TrackInfo track = requestedTrack;
        activeDecoder->setExternalAbortFlag(&abortDecode_);

        if (track.isOnline()) {
            OnlineStreamResolverFn resolver;
            {
                std::lock_guard<std::mutex> lock(resolverMutex_);
                resolver = onlineStreamResolver_;
            }
            if (!resolver) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit errorOccurred(QStringLiteral("Online stream resolver is not configured."));
                }, Qt::QueuedConnection);
                state_.store(PlaybackState::Stopped, std::memory_order_release);
                return;
            }

            ResolvedOnlinePlayback resolved = resolver(track, &abortDecode_);
            if (abortDecode_.load(std::memory_order_acquire)) {
                return;
            }
            if (!resolved.error.isEmpty()
                || (resolved.localCachePath.isEmpty() && !resolved.session && resolved.directUrl.isEmpty())) {
                const QString msg = resolved.error.isEmpty()
                    ? QStringLiteral("Failed to resolve online stream URL.")
                    : resolved.error;
                QMetaObject::invokeMethod(this, [this, msg]() {
                    emit errorOccurred(msg);
                }, Qt::QueuedConnection);
                state_.store(PlaybackState::Stopped, std::memory_order_release);
                return;
            }

            QString openErr;
            bool opened = false;
            if (!resolved.localCachePath.isEmpty()) {
                setStreamSessionAndRatio(nullptr, 1.0);
                opened = activeDecoder->open(resolved.localCachePath, 0, 2, cfg.dsdTargetSampleRate, 0, 0, &openErr);
            } else if (resolved.session) {
                const double initR = resolved.session->isComplete() ? 1.0 : resolved.session->progressRatio();
                setStreamSessionAndRatio(resolved.session, initR);
                opened = activeDecoder->openStream(resolved.session, 0, 2, cfg.dsdTargetSampleRate, &openErr);
            } else {
                setStreamSessionAndRatio(nullptr, 0.0);
                opened = activeDecoder->open(resolved.directUrl, 0, 2, cfg.dsdTargetSampleRate, 0, 0, &openErr);
            }

            if (!opened) {
                setStreamSessionAndRatio(nullptr, 0.0);
                if (abortDecode_.load(std::memory_order_acquire) || activeDecoder->wasAborted()) {
                    return;
                }
                QMetaObject::invokeMethod(this, [this, openErr]() {
                    emit errorOccurred(openErr);
                }, Qt::QueuedConnection);
                state_.store(PlaybackState::Stopped, std::memory_order_release);
                return;
            }

            // Merge probed audio specifications while keeping queue entry free of transient URLs/paths
            const TrackInfo& probed = activeDecoder->trackInfo();
            if (track.title.isEmpty() && !probed.title.isEmpty()) track.title = probed.title;
            if (track.artist.isEmpty() && !probed.artist.isEmpty()) track.artist = probed.artist;
            if (track.album.isEmpty() && !probed.album.isEmpty()) track.album = probed.album;
            if (track.durationMs <= 0 && probed.durationMs > 0) track.durationMs = probed.durationMs;
            track.sampleRate = probed.sampleRate;
            track.bitDepth = probed.bitDepth;
            track.channels = probed.channels > 0 ? probed.channels : 2;
            track.codec = !probed.codec.isEmpty() ? probed.codec : resolved.codec;
            track.bitrateKbps = probed.bitrateKbps > 0 ? probed.bitrateKbps : resolved.bitrateKbps;
            track.formatBadge = !probed.formatBadge.isEmpty() ? probed.formatBadge : resolved.formatBadge;
            track.isDsd = probed.isDsd;
            track.filePath.clear();

            const int desiredRate = desiredSampleRateForTrack(track, cfg);
            const int desiredCh = std::clamp(track.channels > 0 ? track.channels : 2, 1, 2);
            const bool needOpenOutput = !output_->isOpen()
                || activeSampleRate_.load(std::memory_order_acquire) != desiredRate
                || activeChannels_.load(std::memory_order_acquire) != desiredCh
                || (output_->activeStatus().exclusiveActive != (cfg.mode == OutputMode::WasapiExclusive));

            if (needOpenOutput) {
                output_->stop();
                if (!openOutputForFormat(desiredRate, desiredCh)) {
                    activeDecoder->close();
                    QMetaObject::invokeMethod(this, [this]() {
                        emit errorOccurred(QStringLiteral("Failed to initialize audio output device."));
                    }, Qt::QueuedConnection);
                    state_.store(PlaybackState::Stopped, std::memory_order_release);
                    return;
                }
            } else {
                output_->stop();
                ringBuffer_.reset();
                boundaryQueue_.reset();
                eq_.resetState();
            }

            const int outSr = activeSampleRate_.load(std::memory_order_acquire);
            const int outCh = activeChannels_.load(std::memory_order_acquire);
            QString reconfErr;
            if (!activeDecoder->reconfigureOutput(outSr, outCh, &reconfErr)) {
                activeDecoder->close();
                QMetaObject::invokeMethod(this, [this, reconfErr]() {
                    emit errorOccurred(reconfErr);
                }, Qt::QueuedConnection);
                state_.store(PlaybackState::Stopped, std::memory_order_release);
                return;
            }
        } else {
            if (track.sampleRate <= 0) {
                TrackInfo probed;
                if (FFmpegDecoder::probeFile(track.filePath, probed)) {
                    if (!track.isCue) {
                        probed.id = track.id;
                        probed.isFavorite = track.isFavorite;
                        probed.coverHash = track.coverHash;
                        track = probed;
                    } else {
                        track.sampleRate = probed.sampleRate;
                        track.channels = probed.channels;
                        track.bitDepth = probed.bitDepth;
                        track.codec = probed.codec;
                        track.formatBadge = probed.formatBadge;
                        track.isDsd = probed.isDsd;
                        if (track.durationMs <= 0 && track.endMs > track.startMs) {
                            track.durationMs = track.endMs - track.startMs;
                        }
                    }
                }
            }

            const int desiredRate = desiredSampleRateForTrack(track, cfg);
            const int desiredCh = std::clamp(track.channels > 0 ? track.channels : 2, 1, 2);

            const bool needOpenOutput = !output_->isOpen()
                || activeSampleRate_.load(std::memory_order_acquire) != desiredRate
                || activeChannels_.load(std::memory_order_acquire) != desiredCh
                || (output_->activeStatus().exclusiveActive != (cfg.mode == OutputMode::WasapiExclusive));

            if (needOpenOutput) {
                output_->stop();
                if (!openOutputForFormat(desiredRate, desiredCh)) {
                    QMetaObject::invokeMethod(this, [this]() {
                        emit errorOccurred(QStringLiteral("Failed to initialize audio output device."));
                    }, Qt::QueuedConnection);
                    return;
                }
            } else {
                output_->stop();
                ringBuffer_.reset();
                boundaryQueue_.reset();
                eq_.resetState();
            }

            const int outSr = activeSampleRate_.load(std::memory_order_acquire);
            const int outCh = activeChannels_.load(std::memory_order_acquire);

            QString err;
            if (!activeDecoder->open(track.filePath, outSr, outCh, cfg.dsdTargetSampleRate,
                                     track.startMs, track.endMs, &err)) {
                QMetaObject::invokeMethod(this, [this, err]() {
                    emit errorOccurred(err);
                }, Qt::QueuedConnection);
                state_.store(PlaybackState::Stopped, std::memory_order_release);
                return;
            }

            // Merge probed technical details back if queue had minimal TrackInfo
            if (track.durationMs <= 0) {
                track.durationMs = activeDecoder->trackInfo().durationMs;
            }
            if (track.codec.isEmpty()) {
                track.codec = activeDecoder->trackInfo().codec;
                track.formatBadge = activeDecoder->trackInfo().formatBadge;
                track.sampleRate = activeDecoder->trackInfo().sampleRate;
                track.bitDepth = activeDecoder->trackInfo().bitDepth;
                track.bitrateKbps = activeDecoder->trackInfo().bitrateKbps;
                track.isDsd = activeDecoder->trackInfo().isDsd;
            }
            if (!track.hasRgTrack && activeDecoder->trackInfo().hasRgTrack) {
                track.rgTrackGainDb = activeDecoder->trackInfo().rgTrackGainDb;
                track.rgTrackPeak = activeDecoder->trackInfo().rgTrackPeak;
                track.hasRgTrack = true;
            }
            if (!track.hasRgAlbum && activeDecoder->trackInfo().hasRgAlbum) {
                track.rgAlbumGainDb = activeDecoder->trackInfo().rgAlbumGainDb;
                track.rgAlbumPeak = activeDecoder->trackInfo().rgAlbumPeak;
                track.hasRgAlbum = true;
            }
            if (track.embeddedLyrics.isEmpty()) {
                track.embeddedLyrics = activeDecoder->trackInfo().embeddedLyrics;
            }
        }

        const int outSr = activeSampleRate_.load(std::memory_order_acquire);

        if (initialSeekMs > 0) {
            activeDecoder->seekRelativeMs(initialSeekMs);
        }

        activeTrack = track;
        activeQueueIdx = qIdx;

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            currentTrack_ = track;
            if (qIdx >= 0 && qIdx < queue_.size()) {
                queue_[qIdx] = track;
            }
        }

        activeDurationMs_.store(track.durationMs, std::memory_order_release);
        trackBaseMs_.store(initialSeekMs, std::memory_order_release);
        trackStartAbsFrame_.store(ringBuffer_.totalReadFrames(), std::memory_order_release);
        playedAbsFrames_.store(ringBuffer_.totalReadFrames(), std::memory_order_release);
        rtRgGain_ = static_cast<float>(ReplayGainProcessor::computeLinearGain(
            track, replayGainMode_.load(std::memory_order_acquire), rgPreampDb_.load(std::memory_order_acquire)));

        // Pre-fill ~150 ms before starting audio device so startup is instant & glitch-free
        const size_t prefillTarget = std::min<size_t>(
            ringBuffer_.capacityFrames() / 2,
            static_cast<size_t>(outSr) / 6);
        while (ringBuffer_.availableRead() < prefillTarget && !activeEof && !abortDecode_.load(std::memory_order_acquire)) {
            const size_t n = activeDecoder->decodeFrames(decodeChunk.data(), 2048, activeEof);
            if (n > 0) {
                ringBuffer_.write(decodeChunk.data(), n);
            }
        }

        rtFadeGain_ = 0.0f;
        rtFadeSamplesRemaining_ = std::max(64, outSr / 250);
        rtPaused_.store(false, std::memory_order_release);
        output_->start();
        state_.store(PlaybackState::Playing, std::memory_order_release);

        QMetaObject::invokeMethod(this, [this, track, qIdx]() {
            emit currentTrackChanged(track, qIdx);
            emit durationChanged(track.durationMs);
            emit stateChanged(PlaybackState::Playing);
            emit positionChanged(trackBaseMs_.load(std::memory_order_acquire));
            emit trackStartedPlayback(track);
            if (track.id >= 0) {
                emit trackPlaybackStarted(track.id);
            }
        }, Qt::QueuedConnection);
    };

    while (decodeRunning_.load(std::memory_order_acquire)) {
        // 1. Handle any pending control commands first
        DecodeCommand cmd{};
        {
            std::unique_lock<std::mutex> lock(cmdMutex_);
            if (!hasPendingCmd_ && (!activeDecoder->isOpen() || eosPushed || waitingReconfigure)) {
                cmdCv_.wait_for(lock, std::chrono::milliseconds(50), [&]() {
                    return hasPendingCmd_ || !decodeRunning_.load(std::memory_order_acquire);
                });
            }
            if (hasPendingCmd_) {
                cmd = pendingCmd_;
                hasPendingCmd_ = false;
            }
        }

        if (cmd.type == DecodeCommandType::Shutdown || !decodeRunning_.load(std::memory_order_acquire)) {
            break;
        }
        if (cmd.type == DecodeCommandType::Stop) {
            output_->stop();
            activeDecoder->close();
            nextDecoder->close();
            ringBuffer_.reset();
            boundaryQueue_.reset();
            setStreamSessionAndRatio(nullptr, 0.0);
            activeEof = false;
            eosPushed = false;
            waitingReconfigure = false;
            abortDecode_.store(false, std::memory_order_release);
            continue;
        }
        if (cmd.type == DecodeCommandType::OpenTrack || cmd.type == DecodeCommandType::ReconfigureOutput) {
            openAndStartTrack(cmd.track, cmd.queueIndex, cmd.seekMs);
            continue;
        }
        if (cmd.type == DecodeCommandType::Seek && activeDecoder->isOpen()) {
            abortDecode_.store(false, std::memory_order_release);
            nextDecoder->close();
            preloadedQueueIdx = -1;
            activeEof = false;
            eosPushed = false;
            waitingReconfigure = false;

            output_->stop();
            activeDecoder->seekRelativeMs(cmd.seekMs);
            ringBuffer_.reset();
            boundaryQueue_.reset();
            eq_.resetState();
            trackBaseMs_.store(cmd.seekMs, std::memory_order_release);
            trackStartAbsFrame_.store(0, std::memory_order_release);
            playedAbsFrames_.store(0, std::memory_order_release);

            const int outSr = activeSampleRate_.load(std::memory_order_acquire);
            const size_t prefillTarget = std::min<size_t>(ringBuffer_.capacityFrames() / 2, static_cast<size_t>(outSr) / 8);
            while (ringBuffer_.availableRead() < prefillTarget && !activeEof && !abortDecode_.load(std::memory_order_acquire)) {
                const size_t n = activeDecoder->decodeFrames(decodeChunk.data(), 2048, activeEof);
                if (n > 0) ringBuffer_.write(decodeChunk.data(), n);
            }
            if (!rtPaused_.load(std::memory_order_acquire)) {
                rtFadeGain_ = 0.0f;
                rtFadeSamplesRemaining_ = std::max(64, outSr / 200);
                output_->start();
            }
            continue;
        }

        // 2. Active streaming decode loop
        if (!activeDecoder->isOpen() || eosPushed || waitingReconfigure) {
            continue;
        }

        // Wait gently if ring buffer is sufficiently full
        const size_t freeFrames = ringBuffer_.availableWrite();
        if (freeFrames < 2048) {
            std::unique_lock<std::mutex> lock(cmdMutex_);
            cmdCv_.wait_for(lock, std::chrono::milliseconds(8), [&]() {
                return hasPendingCmd_ || !decodeRunning_.load(std::memory_order_acquire);
            });
            continue;
        }

        if (!activeEof) {
            const size_t req = std::min<size_t>(2048, freeFrames);
            const size_t decoded = activeDecoder->decodeFrames(decodeChunk.data(), req, activeEof);
            if (decoded > 0) {
                ringBuffer_.write(decodeChunk.data(), decoded);
            }

            // Preload next track for gapless transition when within 5s of end (non-contiguous CUE)
            const int64_t remainingMs = activeTrack.durationMs > 0
                ? (activeTrack.durationMs - activeDecoder->currentTimelineMs())
                : -1;
            if (!nextDecoder->isOpen() && preloadedQueueIdx < 0 && remainingMs >= 0 && remainingMs <= 5000) {
                int candidateIdx = -1;
                TrackInfo candidate;
                OutputConfig cfg;
                {
                    std::lock_guard<std::mutex> lock(queueMutex_);
                    candidateIdx = computeNextIndexLocked(false, activeQueueIdx);
                    if (candidateIdx >= 0 && candidateIdx < queue_.size()) {
                        candidate = queue_[candidateIdx];
                    }
                    cfg = outputConfig_;
                }
                if (candidateIdx >= 0 && !candidate.isOnline() && !isContiguousCuePair(activeTrack, candidate)) {
                    const int curSr = activeSampleRate_.load(std::memory_order_acquire);
                    const int candDesiredSr = desiredSampleRateForTrack(candidate, cfg);
                    const bool rateMatch = (cfg.mode != OutputMode::WasapiExclusive || !cfg.autoNativeSampleRate || candDesiredSr == curSr);
                    if (rateMatch) {
                        const int outCh = activeChannels_.load(std::memory_order_acquire);
                        if (nextDecoder->open(candidate.filePath, curSr, outCh, cfg.dsdTargetSampleRate,
                                              candidate.startMs, candidate.endMs)) {
                            preloadedQueueIdx = candidateIdx;
                            preloadedTrack = candidate;
                        }
                    }
                }
            }
            continue;
        }

        // 3. activeEof == true -> Seamless Gapless Handover or Contiguous CUE Continuation or Rate Switch!
        int nextIdx = -1;
        TrackInfo nextTrack;
        OutputConfig cfg;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            nextIdx = computeNextIndexLocked(false, activeQueueIdx);
            if (nextIdx >= 0 && nextIdx < queue_.size()) {
                nextTrack = queue_[nextIdx];
            }
            cfg = outputConfig_;
        }

        if (nextIdx < 0) {
            StreamBoundaryMarker eos{};
            eos.absoluteFrame = ringBuffer_.totalWrittenFrames();
            eos.isEndOfStream = true;
            boundaryQueue_.push(eos);
            eosPushed = true;
            continue;
        }

        const int curSr = activeSampleRate_.load(std::memory_order_acquire);
        const int curCh = activeChannels_.load(std::memory_order_acquire);

        // Case A: Contiguous CUE subtrack on the EXACT same backing file -> zero reopen, sample-accurate!
        if (isContiguousCuePair(activeTrack, nextTrack)) {
            activeDecoder->advanceContiguousCueWindow(nextTrack.startMs, nextTrack.endMs);
            activeTrack = nextTrack;
            activeQueueIdx = nextIdx;
            activeEof = false;

            StreamBoundaryMarker m{};
            m.absoluteFrame = ringBuffer_.totalWrittenFrames();
            m.queueIndex = nextIdx;
            m.trackId = nextTrack.id;
            m.sampleRate = curSr;
            m.channels = curCh;
            m.rgLinearGain = ReplayGainProcessor::computeLinearGain(
                nextTrack, replayGainMode_.load(std::memory_order_acquire), rgPreampDb_.load(std::memory_order_acquire));
            boundaryQueue_.push(m);
            continue;
        }

        // Case B: Online track (resolve fresh direct URL at transition) or WASAPI Exclusive native rate switch
        const int nextDesiredSr = desiredSampleRateForTrack(nextTrack, cfg);
        if (nextTrack.isOnline()
            || (cfg.mode == OutputMode::WasapiExclusive && cfg.autoNativeSampleRate && nextDesiredSr != curSr)) {
            StreamBoundaryMarker m{};
            m.absoluteFrame = ringBuffer_.totalWrittenFrames();
            m.queueIndex = nextIdx;
            m.trackId = nextTrack.id;
            m.needsDeviceReconfigure = true;
            boundaryQueue_.push(m);
            waitingReconfigure = true;
            continue;
        }

        // Case C: Standard Gapless handover using preloaded decoder (O(1) pointer swap, 0 ms gap)
        bool ready = (preloadedQueueIdx == nextIdx && nextDecoder->isOpen());
        if (!ready) {
            nextDecoder->close();
            ready = nextDecoder->open(nextTrack.filePath, curSr, curCh, cfg.dsdTargetSampleRate,
                                      nextTrack.startMs, nextTrack.endMs);
            if (ready) {
                preloadedTrack = nextTrack;
                preloadedQueueIdx = nextIdx;
            }
        }

        if (ready) {
            // Merge any newly probed info from preloaded decoder
            TrackInfo merged = preloadedTrack;
            if (merged.durationMs <= 0) {
                merged.durationMs = nextDecoder->trackInfo().durationMs;
            }
            if (merged.codec.isEmpty()) {
                merged.codec = nextDecoder->trackInfo().codec;
                merged.formatBadge = nextDecoder->trackInfo().formatBadge;
                merged.sampleRate = nextDecoder->trackInfo().sampleRate;
                merged.bitDepth = nextDecoder->trackInfo().bitDepth;
                merged.bitrateKbps = nextDecoder->trackInfo().bitrateKbps;
                merged.isDsd = nextDecoder->trackInfo().isDsd;
            }
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                if (nextIdx >= 0 && nextIdx < queue_.size()) {
                    queue_[nextIdx] = merged;
                }
            }

            StreamBoundaryMarker m{};
            m.absoluteFrame = ringBuffer_.totalWrittenFrames();
            m.queueIndex = nextIdx;
            m.trackId = merged.id;
            m.sampleRate = curSr;
            m.channels = curCh;
            m.rgLinearGain = ReplayGainProcessor::computeLinearGain(
                merged, replayGainMode_.load(std::memory_order_acquire), rgPreampDb_.load(std::memory_order_acquire));
            boundaryQueue_.push(m);

            activeDecoder->close();
            std::swap(activeDecoder, nextDecoder);
            activeTrack = merged;
            activeQueueIdx = nextIdx;
            preloadedQueueIdx = -1;
            activeEof = false;
            continue;
        }

        // Fallback if next file failed to open
        StreamBoundaryMarker eos{};
        eos.absoluteFrame = ringBuffer_.totalWrittenFrames();
        eos.isEndOfStream = true;
        boundaryQueue_.push(eos);
        eosPushed = true;
    }

    activeDecoder->close();
    nextDecoder->close();
}

void PlaybackEngine::onUiTick() {
    if (pendingReconfigureFlag_.exchange(false, std::memory_order_acq_rel)) {
        handlePendingDeviceReconfigure();
        return;
    }
    if (pendingTransitionFlag_.exchange(false, std::memory_order_acq_rel)) {
        handlePendingTrackTransition();
    }
    if (pendingEosFlag_.exchange(false, std::memory_order_acq_rel)) {
        stop();
        return;
    }

    const int64_t pos = positionMs();
    if (pos != lastEmittedPosMs_) {
        lastEmittedPosMs_ = pos;
        emit positionChanged(pos);
    }

    std::shared_ptr<StreamCacheSession> sess;
    {
        std::lock_guard<std::mutex> lock(streamSessionMutex_);
        sess = activeStreamSession_;
    }
    if (sess) {
        const double r = sess->isComplete() ? 1.0 : std::clamp(sess->progressRatio(), 0.0, 1.0);
        bufferedRatio_.store(r, std::memory_order_release);
    }
    const double curBuf = bufferedRatio_.load(std::memory_order_acquire);
    if (std::abs(curBuf - lastEmittedBufferedRatio_) >= 0.005
        || (curBuf == 0.0 && lastEmittedBufferedRatio_ != 0.0)
        || (curBuf >= 1.0 && lastEmittedBufferedRatio_ < 1.0)) {
        lastEmittedBufferedRatio_ = curBuf;
        emit bufferedRatioChanged(curBuf);
    }
}

void PlaybackEngine::handlePendingTrackTransition() {
    const int idx = pendingTransitionQueueIndex_.load(std::memory_order_acquire);
    TrackInfo t;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (idx < 0 || idx >= queue_.size()) return;
        currentQueueIndex_ = idx;
        currentTrack_ = queue_[idx];
        t = currentTrack_;
    }
    activeDurationMs_.store(t.durationMs, std::memory_order_release);
    emit currentTrackChanged(t, idx);
    emit durationChanged(t.durationMs);
    if (t.id >= 0) {
        emit trackPlaybackStarted(t.id);
    }
}

void PlaybackEngine::handlePendingDeviceReconfigure() {
    const int idx = pendingReconfigureIndex_.load(std::memory_order_acquire);
    if (idx >= 0) {
        playIndex(idx);
    }
}

} // namespace virela
