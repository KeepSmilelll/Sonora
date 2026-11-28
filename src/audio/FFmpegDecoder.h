#pragma once

#include "audio/AudioTypes.h"

#include <QByteArray>
#include <QString>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVIOContext;
struct AVPacket;
struct SwrContext;

namespace virela {

class StreamCacheSession;

class FFmpegDecoder {
public:
    FFmpegDecoder() = default;
    ~FFmpegDecoder();

    FFmpegDecoder(const FFmpegDecoder&) = delete;
    FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;

    // Opens a local audio file (or CUE virtual subtrack range [startMs, endMs]),
    // or an HTTP(S) URL with AVIOInterruptCB + timeout protection.
    // targetSampleRate == 0 keeps track native sample rate (or uses dsdTargetSampleRate for DSD).
    bool open(const QString& filePath,
              int targetSampleRate = 0,
              int targetChannels = 2,
              int dsdTargetSampleRate = 176400,
              int64_t startMs = 0,
              int64_t endMs = 0,
              QString* errorOut = nullptr);

    // Opens a progressive StreamCacheSession via custom AVIOContext (supports Seek + zero-traffic cache).
    bool openStream(std::shared_ptr<StreamCacheSession> session,
                    int targetSampleRate = 0,
                    int targetChannels = 2,
                    int dsdTargetSampleRate = 176400,
                    QString* errorOut = nullptr);

    // Reconfigures output resampler sample rate / channel count without closing AVFormatContext.
    bool reconfigureOutput(int targetSampleRate, int targetChannels, QString* errorOut = nullptr);

    void setExternalAbortFlag(const std::atomic<bool>* flag) noexcept { externalAbort_ = flag; }
    void requestAbort() noexcept { abortRequested_.store(true, std::memory_order_release); }
    void setNetworkTimeoutMs(int timeoutMs) noexcept { networkTimeoutMs_ = timeoutMs > 0 ? timeoutMs : 12000; }
    bool wasAborted() const noexcept;
    bool wasTimedOut() const noexcept { return timedOut_.load(std::memory_order_acquire); }

    void close();
    bool isOpen() const noexcept { return fmtCtx_ != nullptr; }

    // Decodes up to maxFrames interleaved float PCM frames into outInterleaved.
    // Sets reachedEof = true when the file or CUE subtrack endMs is exhausted.
    size_t decodeFrames(float* outInterleaved, size_t maxFrames, bool& reachedEof);

    // Seeks to position relative to startMs_ (0 = start of subtrack).
    bool seekRelativeMs(int64_t relativeMs);

    // For contiguous CUE tracks on the same backing file: seamlessly updates the
    // end boundary without reopening AVFormatContext or flushing the codec.
    void advanceContiguousCueWindow(int64_t nextStartMs, int64_t nextEndMs);

    const TrackInfo& trackInfo() const noexcept { return info_; }
    int outputSampleRate() const noexcept { return outSampleRate_; }
    int outputChannels() const noexcept { return outChannels_; }
    int64_t currentTimelineMs() const noexcept;
    const QString& filePath() const noexcept { return filePath_; }
    int64_t subtrackStartMs() const noexcept { return startMs_; }
    int64_t subtrackEndMs() const noexcept { return endMs_; }

    // Lightweight metadata & technical audio probe (never decodes audio frames).
    static bool probeFile(const QString& filePath, TrackInfo& outInfo, QString* errorOut = nullptr);

    // Extracts embedded cover art packet bytes (APIC / attached picture) if present.
    static QByteArray extractEmbeddedCover(const QString& filePath);

private:
    bool initResampler(int targetSampleRate, int targetChannels, QString* errorOut);
    bool finishOpenCommon(const QString& sourceLabel,
                          int targetSampleRate,
                          int targetChannels,
                          int dsdTargetSampleRate,
                          QString* errorOut);
    size_t drainPending(float* dst, size_t maxFrames);
    void appendResampledFrames(const float* src, size_t frameCount);
    void discardFramesForPreRoll(size_t& frameCount, const float*& ptr);

    void armIoDeadline() noexcept;
    void disarmIoDeadline() noexcept;
    static int avioInterruptCallback(void* opaque) noexcept;
    static int avioReadPacket(void* opaque, uint8_t* buf, int bufSize);
    static int64_t avioSeek(void* opaque, int64_t offset, int whence);

    AVFormatContext* fmtCtx_ = nullptr;
    AVIOContext* avioCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;

    std::shared_ptr<StreamCacheSession> streamSession_;
    int64_t streamOffset_ = 0;
    bool isNetworkInput_ = false;
    int networkTimeoutMs_ = 12000;
    const std::atomic<bool>* externalAbort_ = nullptr;
    std::atomic<bool> abortRequested_{false};
    std::atomic<bool> timedOut_{false};
    std::atomic<int64_t> ioDeadlineMs_{0};

    int audioStreamIndex_ = -1;
    int outSampleRate_ = 48000;
    int outChannels_ = 2;

    QString filePath_;
    TrackInfo info_;
    int64_t startMs_ = 0;
    int64_t endMs_ = 0;

    int64_t decodedStreamFrames_ = 0; // frames produced relative to file timeline
    int64_t framesToSkip_ = 0;        // sample-accurate seek/CUE leading trim
    int64_t maxSubtrackFrames_ = -1;  // -1 = until EOF
    int64_t emittedSubtrackFrames_ = 0;
    bool decoderFlushed_ = false;
    bool eofReached_ = false;

    std::vector<float> pendingBuffer_;
    size_t pendingReadOffset_ = 0;
    std::vector<float> convertScratch_;
};

} // namespace virela
