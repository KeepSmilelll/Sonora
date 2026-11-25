#pragma once

#include "audio/AudioTypes.h"

#include <QByteArray>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <vector>

struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwrContext;

namespace sonora {

class FFmpegDecoder {
public:
    FFmpegDecoder() = default;
    ~FFmpegDecoder();

    FFmpegDecoder(const FFmpegDecoder&) = delete;
    FFmpegDecoder& operator=(const FFmpegDecoder&) = delete;

    // Opens a local audio file (or CUE virtual subtrack range [startMs, endMs]).
    // targetSampleRate == 0 keeps track native sample rate (or uses dsdTargetSampleRate for DSD).
    bool open(const QString& filePath,
              int targetSampleRate = 0,
              int targetChannels = 2,
              int dsdTargetSampleRate = 176400,
              int64_t startMs = 0,
              int64_t endMs = 0,
              QString* errorOut = nullptr);

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
    size_t drainPending(float* dst, size_t maxFrames);
    void appendResampledFrames(const float* src, size_t frameCount);
    void discardFramesForPreRoll(size_t& frameCount, const float*& ptr);

    AVFormatContext* fmtCtx_ = nullptr;
    AVCodecContext* codecCtx_ = nullptr;
    SwrContext* swrCtx_ = nullptr;
    AVPacket* packet_ = nullptr;
    AVFrame* frame_ = nullptr;

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

} // namespace sonora
