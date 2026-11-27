#include "audio/FFmpegDecoder.h"
#include "core/TextEncodingHelper.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/opt.h>
#include <libavutil/replaygain.h>
#include <libswresample/swresample.h>
}

namespace virela {

namespace {

QString ffErrorString(int errnum) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

bool isDsdCodecId(AVCodecID id) {
    return id == AV_CODEC_ID_DSD_LSBF
        || id == AV_CODEC_ID_DSD_MSBF
        || id == AV_CODEC_ID_DSD_LSBF_PLANAR
        || id == AV_CODEC_ID_DSD_MSBF_PLANAR;
}

bool isLosslessCodecId(AVCodecID id) {
    switch (id) {
    case AV_CODEC_ID_FLAC:
    case AV_CODEC_ID_ALAC:
    case AV_CODEC_ID_APE:
    case AV_CODEC_ID_WAVPACK:
    case AV_CODEC_ID_TTA:
    case AV_CODEC_ID_WMALOSSLESS:
    case AV_CODEC_ID_MLP:
    case AV_CODEC_ID_TRUEHD:
    case AV_CODEC_ID_PCM_S16LE:
    case AV_CODEC_ID_PCM_S16BE:
    case AV_CODEC_ID_PCM_S24LE:
    case AV_CODEC_ID_PCM_S24BE:
    case AV_CODEC_ID_PCM_S32LE:
    case AV_CODEC_ID_PCM_S32BE:
    case AV_CODEC_ID_PCM_F32LE:
    case AV_CODEC_ID_PCM_F32BE:
    case AV_CODEC_ID_PCM_F64LE:
    case AV_CODEC_ID_PCM_U8:
        return true;
    default:
        return false;
    }
}

QString prettyCodecName(AVCodecID id) {
    switch (id) {
    case AV_CODEC_ID_FLAC: return QStringLiteral("FLAC");
    case AV_CODEC_ID_ALAC: return QStringLiteral("ALAC");
    case AV_CODEC_ID_APE: return QStringLiteral("APE");
    case AV_CODEC_ID_WAVPACK: return QStringLiteral("WavPack");
    case AV_CODEC_ID_TTA: return QStringLiteral("TTA");
    case AV_CODEC_ID_MP3: return QStringLiteral("MP3");
    case AV_CODEC_ID_AAC: return QStringLiteral("AAC");
    case AV_CODEC_ID_VORBIS: return QStringLiteral("Vorbis");
    case AV_CODEC_ID_OPUS: return QStringLiteral("Opus");
    case AV_CODEC_ID_WMAV1:
    case AV_CODEC_ID_WMAV2:
    case AV_CODEC_ID_WMAPRO: return QStringLiteral("WMA");
    case AV_CODEC_ID_WMALOSSLESS: return QStringLiteral("WMA Lossless");
    case AV_CODEC_ID_DSD_LSBF:
    case AV_CODEC_ID_DSD_MSBF:
    case AV_CODEC_ID_DSD_LSBF_PLANAR:
    case AV_CODEC_ID_DSD_MSBF_PLANAR:
        return QStringLiteral("DSD");
    default: {
        if (id >= AV_CODEC_ID_FIRST_AUDIO && id <= AV_CODEC_ID_PCM_F64LE) {
            return QStringLiteral("PCM");
        }
        const char* name = avcodec_get_name(id);
        return name ? QString::fromUtf8(name).toUpper() : QStringLiteral("AUDIO");
    }
    }
}

QString readTag(const AVDictionary* dict, const char* key) {
    if (!dict) {
        return {};
    }
    AVDictionaryEntry* entry = av_dict_get(dict, key, nullptr, 0);
    if (!entry || !entry->value) {
        return {};
    }
    return TextEncodingHelper::repairMetadataString(QString::fromUtf8(entry->value)).trimmed();
}

QString readTagMerged(const AVDictionary* fmtDict, const AVDictionary* streamDict, const char* key) {
    QString v = readTag(fmtDict, key);
    if (v.isEmpty()) {
        v = readTag(streamDict, key);
    }
    return v;
}

QString readEmbeddedLyrics(const AVDictionary* fmtDict, const AVDictionary* streamDict) {
    auto scanDict = [](const AVDictionary* dict) -> QString {
        if (!dict) return {};
        const AVDictionaryEntry* tag = nullptr;
        while ((tag = av_dict_get(dict, "", tag, AV_DICT_IGNORE_SUFFIX)) != nullptr) {
            const QString key = QString::fromUtf8(tag->key).toLower();
            if (key == QStringLiteral("lyrics")
                || key.startsWith(QStringLiteral("lyrics-"))
                || key == QStringLiteral("unsyncedlyrics")
                || key == QStringLiteral("uslt")
                || key == QStringLiteral("sylt")) {
                QString val = TextEncodingHelper::repairMetadataString(QString::fromUtf8(tag->value)).trimmed();
                if (!val.isEmpty()) {
                    return val;
                }
            }
        }
        return {};
    };
    QString lrc = scanDict(fmtDict);
    if (lrc.isEmpty()) {
        lrc = scanDict(streamDict);
    }
    return lrc;
}

int parseLeadingInt(const QString& text) {
    if (text.isEmpty()) return 0;
    const int slash = text.indexOf(QLatin1Char('/'));
    const QString part = slash >= 0 ? text.left(slash) : text;
    bool ok = false;
    int val = part.trimmed().toInt(&ok);
    return ok ? val : 0;
}

bool parseDbValue(const QString& text, double& outDb) {
    if (text.isEmpty()) return false;
    static const QRegularExpression re(QStringLiteral("([+-]?\\d+(?:\\.\\d+)?)"));
    const auto m = re.match(text);
    if (!m.hasMatch()) return false;
    bool ok = false;
    double val = m.captured(1).toDouble(&ok);
    if (ok) outDb = val;
    return ok;
}

void extractReplayGain(const AVFormatContext* fmtCtx, const AVStream* stream, TrackInfo& info) {
    // Priority 1: FFmpeg 7+/8+/9+ stream coded_side_data AV_PKT_DATA_REPLAYGAIN
    if (stream && stream->codecpar && stream->codecpar->coded_side_data) {
        const AVPacketSideData* sd = av_packet_side_data_get(
            stream->codecpar->coded_side_data,
            stream->codecpar->nb_coded_side_data,
            AV_PKT_DATA_REPLAYGAIN);
        if (sd && sd->size >= sizeof(AVReplayGain)) {
            const auto* rg = reinterpret_cast<const AVReplayGain*>(sd->data);
            if (rg->track_gain != std::numeric_limits<int32_t>::min()) {
                info.rgTrackGainDb = static_cast<double>(rg->track_gain) / 100000.0;
                info.hasRgTrack = true;
            }
            if (rg->track_peak != 0) {
                info.rgTrackPeak = static_cast<double>(rg->track_peak) / 100000.0;
            }
            if (rg->album_gain != std::numeric_limits<int32_t>::min()) {
                info.rgAlbumGainDb = static_cast<double>(rg->album_gain) / 100000.0;
                info.hasRgAlbum = true;
            }
            if (rg->album_peak != 0) {
                info.rgAlbumPeak = static_cast<double>(rg->album_peak) / 100000.0;
            }
        }
    }

    // Priority 2: Standard ReplayGain / R128 metadata tags
    const AVDictionary* fmtMeta = fmtCtx ? fmtCtx->metadata : nullptr;
    const AVDictionary* stMeta = stream ? stream->metadata : nullptr;

    if (!info.hasRgTrack) {
        double db = 0.0;
        if (parseDbValue(readTagMerged(fmtMeta, stMeta, "REPLAYGAIN_TRACK_GAIN"), db)) {
            info.rgTrackGainDb = db;
            info.hasRgTrack = true;
        } else {
            // Opus R128_TRACK_GAIN is Q7.8 fixed-point relative to -23 LUFS (+5 dB to match RG -18 LUFS)
            QString r128 = readTagMerged(fmtMeta, stMeta, "R128_TRACK_GAIN");
            bool ok = false;
            int q78 = r128.toInt(&ok);
            if (ok) {
                info.rgTrackGainDb = (static_cast<double>(q78) / 256.0) + 5.0;
                info.hasRgTrack = true;
            }
        }
    }
    if (!info.hasRgAlbum) {
        double db = 0.0;
        if (parseDbValue(readTagMerged(fmtMeta, stMeta, "REPLAYGAIN_ALBUM_GAIN"), db)) {
            info.rgAlbumGainDb = db;
            info.hasRgAlbum = true;
        } else {
            QString r128 = readTagMerged(fmtMeta, stMeta, "R128_ALBUM_GAIN");
            bool ok = false;
            int q78 = r128.toInt(&ok);
            if (ok) {
                info.rgAlbumGainDb = (static_cast<double>(q78) / 256.0) + 5.0;
                info.hasRgAlbum = true;
            }
        }
    }

    double peak = 1.0;
    if (parseDbValue(readTagMerged(fmtMeta, stMeta, "REPLAYGAIN_TRACK_PEAK"), peak) && peak > 0.0) {
        info.rgTrackPeak = peak;
    }
    if (parseDbValue(readTagMerged(fmtMeta, stMeta, "REPLAYGAIN_ALBUM_PEAK"), peak) && peak > 0.0) {
        info.rgAlbumPeak = peak;
    }
}

void populateTrackInfoFromContext(const QString& filePath,
                                  AVFormatContext* fmtCtx,
                                  int streamIdx,
                                  TrackInfo& info) {
    const AVStream* stream = fmtCtx->streams[streamIdx];
    const AVCodecParameters* par = stream->codecpar;
    const QFileInfo fi(filePath);

    info.filePath = filePath;
    info.fileSize = fi.size();
    info.fileMtime = fi.lastModified().toSecsSinceEpoch();

    info.title = readTagMerged(fmtCtx->metadata, stream->metadata, "title");
    if (info.title.isEmpty()) {
        info.title = fi.completeBaseName();
    }
    info.artist = readTagMerged(fmtCtx->metadata, stream->metadata, "artist");
    info.album = readTagMerged(fmtCtx->metadata, stream->metadata, "album");
    info.albumArtist = readTagMerged(fmtCtx->metadata, stream->metadata, "album_artist");
    if (info.albumArtist.isEmpty()) {
        info.albumArtist = info.artist;
    }
    info.genre = readTagMerged(fmtCtx->metadata, stream->metadata, "genre");
    info.trackNumber = parseLeadingInt(readTagMerged(fmtCtx->metadata, stream->metadata, "track"));
    info.discNumber = parseLeadingInt(readTagMerged(fmtCtx->metadata, stream->metadata, "disc"));
    info.year = parseLeadingInt(readTagMerged(fmtCtx->metadata, stream->metadata, "date"));
    if (info.year == 0) {
        info.year = parseLeadingInt(readTagMerged(fmtCtx->metadata, stream->metadata, "year"));
    }

    if (fmtCtx->duration != AV_NOPTS_VALUE && fmtCtx->duration > 0) {
        info.durationMs = (fmtCtx->duration * 1000) / AV_TIME_BASE;
    } else if (stream->duration != AV_NOPTS_VALUE && stream->duration > 0) {
        info.durationMs = av_rescale_q(stream->duration, stream->time_base, AVRational{1, 1000});
    }

    info.channels = par->ch_layout.nb_channels > 0 ? par->ch_layout.nb_channels : 2;
    info.codec = prettyCodecName(par->codec_id);
    info.isDsd = isDsdCodecId(par->codec_id);

    if (info.isDsd) {
        // FFmpeg reports DSD stream sample_rate as byte-rate (e.g. 352800 Hz for DSD64 = 2.8224 MHz)
        const int64_t dsdBitRateHz = static_cast<int64_t>(par->sample_rate) * 8;
        info.sampleRate = static_cast<int>(dsdBitRateHz);
        info.bitDepth = 1;
        if (dsdBitRateHz >= 11'000'000) {
            info.formatBadge = QStringLiteral("DSD256");
        } else if (dsdBitRateHz >= 5'500'000) {
            info.formatBadge = QStringLiteral("DSD128");
        } else {
            info.formatBadge = QStringLiteral("DSD64");
        }
    } else {
        info.sampleRate = par->sample_rate > 0 ? par->sample_rate : 44100;
        int bits = par->bits_per_raw_sample;
        if (bits <= 0) {
            bits = av_get_bits_per_sample(par->codec_id);
        }
        if (bits <= 0 && isLosslessCodecId(par->codec_id)) {
            bits = av_get_bytes_per_sample(static_cast<AVSampleFormat>(par->format)) * 8;
        }
        info.bitDepth = bits;

        if (isLosslessCodecId(par->codec_id)) {
            if (info.sampleRate > 48000 || info.bitDepth > 16) {
                info.formatBadge = QStringLiteral("Hi-Res");
            } else {
                info.formatBadge = QStringLiteral("Lossless");
            }
        } else {
            info.formatBadge = info.codec;
        }
    }

    int64_t br = par->bit_rate > 0 ? par->bit_rate : fmtCtx->bit_rate;
    info.bitrateKbps = br > 0 ? static_cast<int>(br / 1000) : 0;

    extractReplayGain(fmtCtx, stream, info);
    info.embeddedLyrics = readEmbeddedLyrics(fmtCtx->metadata, stream->metadata);
}

} // namespace

FFmpegDecoder::~FFmpegDecoder() {
    close();
}

void FFmpegDecoder::close() {
    if (swrCtx_) {
        swr_free(&swrCtx_);
    }
    if (frame_) {
        av_frame_free(&frame_);
    }
    if (packet_) {
        av_packet_free(&packet_);
    }
    if (codecCtx_) {
        avcodec_free_context(&codecCtx_);
    }
    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
    }
    audioStreamIndex_ = -1;
    pendingBuffer_.clear();
    pendingBuffer_.shrink_to_fit();
    pendingReadOffset_ = 0;
    convertScratch_.clear();
    convertScratch_.shrink_to_fit();
    decodedStreamFrames_ = 0;
    framesToSkip_ = 0;
    maxSubtrackFrames_ = -1;
    emittedSubtrackFrames_ = 0;
    decoderFlushed_ = false;
    eofReached_ = false;
}

bool FFmpegDecoder::open(const QString& filePath,
                         int targetSampleRate,
                         int targetChannels,
                         int dsdTargetSampleRate,
                         int64_t startMs,
                         int64_t endMs,
                         QString* errorOut) {
    close();
    filePath_ = filePath;
    startMs_ = std::max<int64_t>(0, startMs);
    endMs_ = std::max<int64_t>(0, endMs);

    const QByteArray pathUtf8 = filePath.toUtf8();
    int ret = avformat_open_input(&fmtCtx_, pathUtf8.constData(), nullptr, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avformat_open_input failed: ") + ffErrorString(ret);
        close();
        return false;
    }

    ret = avformat_find_stream_info(fmtCtx_, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avformat_find_stream_info failed: ") + ffErrorString(ret);
        close();
        return false;
    }

    const AVCodec* decoder = nullptr;
    audioStreamIndex_ = av_find_best_stream(fmtCtx_, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
    if (audioStreamIndex_ < 0 || !decoder) {
        if (errorOut) *errorOut = QStringLiteral("No supported audio stream found");
        close();
        return false;
    }

    AVStream* stream = fmtCtx_->streams[audioStreamIndex_];
    codecCtx_ = avcodec_alloc_context3(decoder);
    if (!codecCtx_) {
        if (errorOut) *errorOut = QStringLiteral("avcodec_alloc_context3 failed");
        close();
        return false;
    }

    ret = avcodec_parameters_to_context(codecCtx_, stream->codecpar);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avcodec_parameters_to_context failed: ") + ffErrorString(ret);
        close();
        return false;
    }

    // Keep low thread count (1) for audio decoding to minimize memory and latency.
    codecCtx_->thread_count = 1;
    ret = avcodec_open2(codecCtx_, decoder, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avcodec_open2 failed: ") + ffErrorString(ret);
        close();
        return false;
    }

    info_ = TrackInfo{};
    populateTrackInfoFromContext(filePath, fmtCtx_, audioStreamIndex_, info_);
    if (endMs_ > startMs_) {
        info_.startMs = startMs_;
        info_.endMs = endMs_;
        info_.durationMs = endMs_ - startMs_;
        info_.isCue = true;
    } else if (startMs_ > 0 && info_.durationMs > startMs_) {
        info_.startMs = startMs_;
        info_.durationMs -= startMs_;
        info_.isCue = true;
    }

    int chosenRate = targetSampleRate;
    if (chosenRate <= 0) {
        if (info_.isDsd) {
            chosenRate = dsdTargetSampleRate > 0 ? dsdTargetSampleRate : 176400;
        } else {
            chosenRate = codecCtx_->sample_rate > 0 ? codecCtx_->sample_rate : 44100;
        }
    }

    int chosenChannels = targetChannels > 0 ? targetChannels : 2;
    if (!initResampler(chosenRate, chosenChannels, errorOut)) {
        close();
        return false;
    }

    packet_ = av_packet_alloc();
    frame_ = av_frame_alloc();
    if (!packet_ || !frame_) {
        if (errorOut) *errorOut = QStringLiteral("Failed to allocate AVPacket/AVFrame");
        close();
        return false;
    }

    if (endMs_ > startMs_) {
        maxSubtrackFrames_ = ((endMs_ - startMs_) * outSampleRate_) / 1000;
    } else {
        maxSubtrackFrames_ = -1;
    }

    if (startMs_ > 0) {
        seekRelativeMs(0);
    }

    return true;
}

bool FFmpegDecoder::initResampler(int targetSampleRate, int targetChannels, QString* errorOut) {
    if (swrCtx_) {
        swr_free(&swrCtx_);
    }

    outSampleRate_ = std::max(8000, targetSampleRate);
    outChannels_ = std::clamp(targetChannels, 1, 8);

    AVChannelLayout inLayout{};
    if (codecCtx_->ch_layout.nb_channels > 0) {
        av_channel_layout_copy(&inLayout, &codecCtx_->ch_layout);
    } else {
        av_channel_layout_default(&inLayout, 2);
    }

    AVChannelLayout outLayout{};
    av_channel_layout_default(&outLayout, outChannels_);

    AVSampleFormat inFmt = codecCtx_->sample_fmt;
    if (inFmt == AV_SAMPLE_FMT_NONE) {
        inFmt = AV_SAMPLE_FMT_FLTP;
    }
    const int inRate = codecCtx_->sample_rate > 0 ? codecCtx_->sample_rate : 44100;

    int ret = swr_alloc_set_opts2(
        &swrCtx_,
        &outLayout, AV_SAMPLE_FMT_FLT, outSampleRate_,
        &inLayout, inFmt, inRate,
        0, nullptr);

    av_channel_layout_uninit(&inLayout);
    av_channel_layout_uninit(&outLayout);

    if (ret < 0 || !swrCtx_) {
        if (errorOut) *errorOut = QStringLiteral("swr_alloc_set_opts2 failed: ") + ffErrorString(ret);
        return false;
    }

    // High-precision resampling settings (especially beneficial for DSD -> PCM and rate changes)
    av_opt_set_int(swrCtx_, "filter_size", info_.isDsd ? 64 : 32, 0);
    av_opt_set_int(swrCtx_, "phase_shift", 10, 0);
    av_opt_set_int(swrCtx_, "dither_method", SWR_DITHER_TRIANGULAR_HIGHPASS, 0);

    ret = swr_init(swrCtx_);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("swr_init failed: ") + ffErrorString(ret);
        swr_free(&swrCtx_);
        return false;
    }
    return true;
}

bool FFmpegDecoder::seekRelativeMs(int64_t relativeMs) {
    if (!fmtCtx_ || !codecCtx_ || audioStreamIndex_ < 0) {
        return false;
    }
    const int64_t clampedRel = std::max<int64_t>(0, relativeMs);
    const int64_t absTargetMs = startMs_ + clampedRel;

    AVStream* stream = fmtCtx_->streams[audioStreamIndex_];
    const int64_t targetTs = av_rescale_q(absTargetMs, AVRational{1, 1000}, stream->time_base);

    // Seek slightly backward to ensure lossless/MP3 bit-reservoir frames decode cleanly
    int ret = av_seek_frame(fmtCtx_, audioStreamIndex_, targetTs, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        ret = av_seek_frame(fmtCtx_, -1, absTargetMs * (AV_TIME_BASE / 1000), AVSEEK_FLAG_BACKWARD);
        if (ret < 0) {
            return false;
        }
    }

    avcodec_flush_buffers(codecCtx_);
    if (swrCtx_) {
        swr_init(swrCtx_);
    }

    pendingBuffer_.clear();
    pendingReadOffset_ = 0;
    decoderFlushed_ = false;
    eofReached_ = false;

    emittedSubtrackFrames_ = (clampedRel * outSampleRate_) / 1000;
    decodedStreamFrames_ = (absTargetMs * outSampleRate_) / 1000;
    framesToSkip_ = -1; // Computed dynamically from first decoded frame PTS
    return true;
}

void FFmpegDecoder::advanceContiguousCueWindow(int64_t nextStartMs, int64_t nextEndMs) {
    startMs_ = std::max<int64_t>(0, nextStartMs);
    endMs_ = std::max<int64_t>(0, nextEndMs);
    emittedSubtrackFrames_ = 0;
    eofReached_ = false;
    if (endMs_ > startMs_) {
        maxSubtrackFrames_ = ((endMs_ - startMs_) * outSampleRate_) / 1000;
    } else {
        maxSubtrackFrames_ = -1;
    }
}

int64_t FFmpegDecoder::currentTimelineMs() const noexcept {
    if (outSampleRate_ <= 0) return 0;
    return (emittedSubtrackFrames_ * 1000) / outSampleRate_;
}

size_t FFmpegDecoder::drainPending(float* dst, size_t maxFrames) {
    const size_t totalAvailSamples = pendingBuffer_.size() - pendingReadOffset_;
    const size_t availFrames = totalAvailSamples / static_cast<size_t>(outChannels_);
    if (availFrames == 0) {
        return 0;
    }
    size_t toCopy = std::min(maxFrames, availFrames);
    if (maxSubtrackFrames_ >= 0) {
        const int64_t rem = std::max<int64_t>(0, maxSubtrackFrames_ - emittedSubtrackFrames_);
        toCopy = std::min(toCopy, static_cast<size_t>(rem));
    }
    if (toCopy > 0) {
        std::memcpy(dst,
                    pendingBuffer_.data() + pendingReadOffset_,
                    toCopy * static_cast<size_t>(outChannels_) * sizeof(float));
        pendingReadOffset_ += toCopy * static_cast<size_t>(outChannels_);
        emittedSubtrackFrames_ += static_cast<int64_t>(toCopy);
    }
    if (pendingReadOffset_ >= pendingBuffer_.size()) {
        pendingBuffer_.clear();
        pendingReadOffset_ = 0;
    }
    return toCopy;
}

void FFmpegDecoder::appendResampledFrames(const float* src, size_t frameCount) {
    if (frameCount == 0) return;
    if (pendingReadOffset_ > 0 && pendingReadOffset_ >= pendingBuffer_.size()) {
        pendingBuffer_.clear();
        pendingReadOffset_ = 0;
    }
    const size_t sampleCount = frameCount * static_cast<size_t>(outChannels_);
    const size_t oldSize = pendingBuffer_.size();
    pendingBuffer_.resize(oldSize + sampleCount);
    std::memcpy(pendingBuffer_.data() + oldSize, src, sampleCount * sizeof(float));
}

void FFmpegDecoder::discardFramesForPreRoll(size_t& frameCount, const float*& ptr) {
    if (framesToSkip_ <= 0 || frameCount == 0) return;
    const size_t skip = std::min<size_t>(frameCount, static_cast<size_t>(framesToSkip_));
    ptr += skip * static_cast<size_t>(outChannels_);
    frameCount -= skip;
    framesToSkip_ -= static_cast<int64_t>(skip);
}

size_t FFmpegDecoder::decodeFrames(float* outInterleaved, size_t maxFrames, bool& reachedEof) {
    reachedEof = false;
    if (!fmtCtx_ || !codecCtx_ || !swrCtx_) {
        reachedEof = true;
        return 0;
    }

    if (maxSubtrackFrames_ >= 0 && emittedSubtrackFrames_ >= maxSubtrackFrames_) {
        reachedEof = true;
        return 0;
    }

    size_t written = drainPending(outInterleaved, maxFrames);

    while (written < maxFrames && !eofReached_) {
        if (maxSubtrackFrames_ >= 0 && emittedSubtrackFrames_ >= maxSubtrackFrames_) {
            reachedEof = true;
            return written;
        }

        int ret = avcodec_receive_frame(codecCtx_, frame_);
        if (ret == AVERROR(EAGAIN)) {
            if (decoderFlushed_) {
                eofReached_ = true;
                break;
            }
            bool packetSent = false;
            while (!packetSent) {
                int readRet = av_read_frame(fmtCtx_, packet_);
                if (readRet == AVERROR_EOF) {
                    avcodec_send_packet(codecCtx_, nullptr);
                    decoderFlushed_ = true;
                    packetSent = true;
                } else if (readRet < 0) {
                    eofReached_ = true;
                    packetSent = true;
                } else {
                    if (packet_->stream_index == audioStreamIndex_) {
                        int sendRet = avcodec_send_packet(codecCtx_, packet_);
                        av_packet_unref(packet_);
                        if (sendRet < 0 && sendRet != AVERROR(EAGAIN)) {
                            // Corrupted frame: skip and keep reading
                            continue;
                        }
                        packetSent = true;
                    } else {
                        av_packet_unref(packet_);
                    }
                }
            }
            continue;
        } else if (ret == AVERROR_EOF) {
            // Flush remaining delayed samples inside SwrContext
            const int delayed = static_cast<int>(swr_get_delay(swrCtx_, outSampleRate_));
            if (delayed > 0) {
                convertScratch_.resize(static_cast<size_t>(delayed + 64) * static_cast<size_t>(outChannels_));
                uint8_t* outPlanes[1] = {reinterpret_cast<uint8_t*>(convertScratch_.data())};
                int outCount = swr_convert(swrCtx_, outPlanes, delayed + 64, nullptr, 0);
                if (outCount > 0) {
                    appendResampledFrames(convertScratch_.data(), static_cast<size_t>(outCount));
                    written += drainPending(outInterleaved + written * static_cast<size_t>(outChannels_),
                                            maxFrames - written);
                }
            }
            eofReached_ = true;
            break;
        } else if (ret < 0) {
            eofReached_ = true;
            break;
        }

        // Compute sample-accurate leading trim on the first frame after a seek/CUE start
        if (framesToSkip_ < 0) {
            const AVStream* stream = fmtCtx_->streams[audioStreamIndex_];
            int64_t framePtsMs = -1;
            if (frame_->best_effort_timestamp != AV_NOPTS_VALUE) {
                framePtsMs = av_rescale_q(frame_->best_effort_timestamp, stream->time_base, AVRational{1, 1000});
            }
            const int64_t targetAbsMs = startMs_ + (emittedSubtrackFrames_ * 1000) / outSampleRate_;
            if (framePtsMs >= 0 && framePtsMs < targetAbsMs) {
                framesToSkip_ = ((targetAbsMs - framePtsMs) * outSampleRate_) / 1000;
            } else {
                framesToSkip_ = 0;
            }
        }

        const int64_t maxOutSamples = av_rescale_rnd(
            swr_get_delay(swrCtx_, codecCtx_->sample_rate) + frame_->nb_samples,
            outSampleRate_,
            std::max(1, codecCtx_->sample_rate),
            AV_ROUND_UP);

        convertScratch_.resize(static_cast<size_t>(maxOutSamples + 32) * static_cast<size_t>(outChannels_));
        uint8_t* outPlanes[1] = {reinterpret_cast<uint8_t*>(convertScratch_.data())};

        const int converted = swr_convert(
            swrCtx_,
            outPlanes,
            static_cast<int>(maxOutSamples + 32),
            const_cast<const uint8_t**>(frame_->extended_data),
            frame_->nb_samples);

        av_frame_unref(frame_);

        if (converted > 0) {
            size_t producedFrames = static_cast<size_t>(converted);
            const float* srcPtr = convertScratch_.data();
            discardFramesForPreRoll(producedFrames, srcPtr);
            if (producedFrames > 0) {
                decodedStreamFrames_ += static_cast<int64_t>(producedFrames);
                appendResampledFrames(srcPtr, producedFrames);
                written += drainPending(outInterleaved + written * static_cast<size_t>(outChannels_),
                                        maxFrames - written);
            }
        }
    }

    if ((eofReached_ && pendingReadOffset_ >= pendingBuffer_.size())
        || (maxSubtrackFrames_ >= 0 && emittedSubtrackFrames_ >= maxSubtrackFrames_)) {
        reachedEof = true;
    }
    return written;
}

bool FFmpegDecoder::probeFile(const QString& filePath, TrackInfo& outInfo, QString* errorOut) {
    AVFormatContext* fmt = nullptr;
    const QByteArray pathUtf8 = filePath.toUtf8();
    int ret = avformat_open_input(&fmt, pathUtf8.constData(), nullptr, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = ffErrorString(ret);
        return false;
    }

    ret = avformat_find_stream_info(fmt, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = ffErrorString(ret);
        avformat_close_input(&fmt);
        return false;
    }

    const int streamIdx = av_find_best_stream(fmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (streamIdx < 0) {
        if (errorOut) *errorOut = QStringLiteral("No audio stream");
        avformat_close_input(&fmt);
        return false;
    }

    outInfo = TrackInfo{};
    populateTrackInfoFromContext(filePath, fmt, streamIdx, outInfo);
    avformat_close_input(&fmt);
    return true;
}

QByteArray FFmpegDecoder::extractEmbeddedCover(const QString& filePath) {
    AVFormatContext* fmt = nullptr;
    const QByteArray pathUtf8 = filePath.toUtf8();
    if (avformat_open_input(&fmt, pathUtf8.constData(), nullptr, nullptr) < 0) {
        return {};
    }

    QByteArray cover;
    for (unsigned i = 0; i < fmt->nb_streams; ++i) {
        const AVStream* st = fmt->streams[i];
        if ((st->disposition & AV_DISPOSITION_ATTACHED_PIC) && st->attached_pic.size > 0 && st->attached_pic.data) {
            cover = QByteArray(reinterpret_cast<const char*>(st->attached_pic.data), st->attached_pic.size);
            break;
        }
    }

    avformat_close_input(&fmt);
    return cover;
}

} // namespace virela
