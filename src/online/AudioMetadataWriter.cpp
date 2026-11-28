#include "online/AudioMetadataWriter.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QUuid>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/dict.h>
#include <libavutil/error.h>
#include <libavutil/pixfmt.h>
}

namespace virela {

namespace {

QString ffErr(int code) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(code, buf, sizeof(buf));
    return QString::fromUtf8(buf);
}

QString muxerNameForExtension(const QString& extLower) {
    if (extLower == QStringLiteral("flac")) return QStringLiteral("flac");
    if (extLower == QStringLiteral("mp3")) return QStringLiteral("mp3");
    if (extLower == QStringLiteral("m4a") || extLower == QStringLiteral("aac")) return QStringLiteral("ipod");
    if (extLower == QStringLiteral("ogg")) return QStringLiteral("ogg");
    if (extLower == QStringLiteral("opus")) return QStringLiteral("opus");
    if (extLower == QStringLiteral("wav")) return QStringLiteral("wav");
    return {};
}

bool supportsAttachedPic(const QString& muxerName) {
    return muxerName == QStringLiteral("flac")
        || muxerName == QStringLiteral("mp3")
        || muxerName == QStringLiteral("ipod");
}

void setTagIfNonEmpty(AVDictionary** dict, const char* key, const QString& value) {
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty()) {
        av_dict_set(dict, key, trimmed.toUtf8().constData(), 0);
    }
}

bool remuxOnce(const QString& srcPath,
               const QString& tmpDstPath,
               const QString& muxerName,
               const TrackInfo& meta,
               const QByteArray& coverBytes,
               const QString& embeddedLyrics,
               bool includeCover,
               QString* errorOut) {
    AVFormatContext* inFmt = nullptr;
    const QByteArray srcUtf8 = srcPath.toUtf8();
    int ret = avformat_open_input(&inFmt, srcUtf8.constData(), nullptr, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avformat_open_input: ") + ffErr(ret);
        return false;
    }

    auto cleanup = [&](AVFormatContext* outFmt, AVPacket* pkt, bool writeTrailer) {
        if (pkt) av_packet_free(&pkt);
        if (outFmt) {
            if (writeTrailer) {
                av_write_trailer(outFmt);
            }
            if (!(outFmt->oformat->flags & AVFMT_NOFILE) && outFmt->pb) {
                avio_closep(&outFmt->pb);
            }
            avformat_free_context(outFmt);
        }
        avformat_close_input(&inFmt);
    };

    ret = avformat_find_stream_info(inFmt, nullptr);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avformat_find_stream_info: ") + ffErr(ret);
        cleanup(nullptr, nullptr, false);
        return false;
    }

    const int inAudioIdx = av_find_best_stream(inFmt, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (inAudioIdx < 0) {
        if (errorOut) *errorOut = QStringLiteral("No audio stream in source file");
        cleanup(nullptr, nullptr, false);
        return false;
    }

    AVFormatContext* outFmt = nullptr;
    const QByteArray dstUtf8 = tmpDstPath.toUtf8();
    const QByteArray fmtUtf8 = muxerName.toUtf8();
    ret = avformat_alloc_output_context2(
        &outFmt,
        nullptr,
        muxerName.isEmpty() ? nullptr : fmtUtf8.constData(),
        dstUtf8.constData());
    if (ret < 0 || !outFmt) {
        if (errorOut) *errorOut = QStringLiteral("avformat_alloc_output_context2: ") + ffErr(ret);
        cleanup(outFmt, nullptr, false);
        return false;
    }

    AVStream* inAudio = inFmt->streams[inAudioIdx];
    AVStream* outAudio = avformat_new_stream(outFmt, nullptr);
    if (!outAudio) {
        if (errorOut) *errorOut = QStringLiteral("avformat_new_stream(audio) failed");
        cleanup(outFmt, nullptr, false);
        return false;
    }
    ret = avcodec_parameters_copy(outAudio->codecpar, inAudio->codecpar);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avcodec_parameters_copy: ") + ffErr(ret);
        cleanup(outFmt, nullptr, false);
        return false;
    }
    outAudio->codecpar->codec_tag = 0;
    outAudio->time_base = inAudio->time_base;

    // Copy existing metadata first, then overwrite with authoritative online metadata
    av_dict_copy(&outFmt->metadata, inFmt->metadata, 0);
    setTagIfNonEmpty(&outFmt->metadata, "title", meta.title);
    setTagIfNonEmpty(&outFmt->metadata, "artist", meta.artist);
    setTagIfNonEmpty(&outFmt->metadata, "album", meta.album);
    setTagIfNonEmpty(&outFmt->metadata, "album_artist", meta.albumArtist.isEmpty() ? meta.artist : meta.albumArtist);
    if (meta.trackNumber > 0) {
        setTagIfNonEmpty(&outFmt->metadata, "track", QString::number(meta.trackNumber));
    }
    if (meta.discNumber > 0) {
        setTagIfNonEmpty(&outFmt->metadata, "disc", QString::number(meta.discNumber));
    }
    if (meta.year > 0) {
        setTagIfNonEmpty(&outFmt->metadata, "date", QString::number(meta.year));
    }
    setTagIfNonEmpty(&outFmt->metadata, "genre", meta.genre);
    setTagIfNonEmpty(&outFmt->metadata, "lyrics", embeddedLyrics);

    // Optional attached cover picture stream
    int picStreamIdx = -1;
    if (includeCover && !coverBytes.isEmpty() && supportsAttachedPic(muxerName)) {
        QBuffer buf;
        buf.setData(coverBytes);
        buf.open(QIODevice::ReadOnly);
        QImageReader reader(&buf);
        const QByteArray imgFmt = reader.format().toLower();
        const QSize imgSize = reader.size();
        const bool isPng = (imgFmt == "png");
        const bool isJpg = (imgFmt == "jpeg" || imgFmt == "jpg");
        if ((isPng || isJpg) && imgSize.isValid()) {
            AVStream* picStream = avformat_new_stream(outFmt, nullptr);
            if (picStream) {
                picStream->disposition |= AV_DISPOSITION_ATTACHED_PIC;
                picStream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
                picStream->codecpar->codec_id = isPng ? AV_CODEC_ID_PNG : AV_CODEC_ID_MJPEG;
                picStream->codecpar->format = isPng ? AV_PIX_FMT_RGBA : AV_PIX_FMT_YUVJ420P;
                picStream->codecpar->width = imgSize.width();
                picStream->codecpar->height = imgSize.height();
                picStream->time_base = AVRational{1, 90000};
                av_dict_set(&picStream->metadata, "comment", "Cover (front)", 0);
                picStreamIdx = picStream->index;
            }
        }
    }

    if (!(outFmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outFmt->pb, dstUtf8.constData(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            if (errorOut) *errorOut = QStringLiteral("avio_open: ") + ffErr(ret);
            cleanup(outFmt, nullptr, false);
            return false;
        }
    }

    AVDictionary* muxOpts = nullptr;
    if (muxerName == QStringLiteral("mp3")) {
        av_dict_set(&muxOpts, "id3v2_version", "3", 0);
    } else if (muxerName == QStringLiteral("ipod")) {
        av_dict_set(&muxOpts, "movflags", "faststart", 0);
    }

    ret = avformat_write_header(outFmt, &muxOpts);
    av_dict_free(&muxOpts);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("avformat_write_header: ") + ffErr(ret);
        cleanup(outFmt, nullptr, false);
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        if (errorOut) *errorOut = QStringLiteral("av_packet_alloc failed");
        cleanup(outFmt, nullptr, true);
        return false;
    }

    if (picStreamIdx >= 0) {
        if (av_new_packet(pkt, static_cast<int>(coverBytes.size())) == 0) {
            std::memcpy(pkt->data, coverBytes.constData(), static_cast<size_t>(coverBytes.size()));
            pkt->stream_index = picStreamIdx;
            pkt->pts = 0;
            pkt->dts = 0;
            pkt->duration = 0;
            pkt->flags |= AV_PKT_FLAG_KEY;
            ret = av_interleaved_write_frame(outFmt, pkt);
            av_packet_unref(pkt);
            if (ret < 0) {
                if (errorOut) *errorOut = QStringLiteral("write attached_pic: ") + ffErr(ret);
                cleanup(outFmt, pkt, false);
                return false;
            }
        }
    }

    while (av_read_frame(inFmt, pkt) >= 0) {
        if (pkt->stream_index != inAudioIdx) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_rescale_ts(pkt, inAudio->time_base, outAudio->time_base);
        pkt->stream_index = outAudio->index;
        pkt->pos = -1;
        if (pkt->dts != AV_NOPTS_VALUE && pkt->pts != AV_NOPTS_VALUE && pkt->dts > pkt->pts) {
            pkt->dts = pkt->pts;
        }
        ret = av_interleaved_write_frame(outFmt, pkt);
        av_packet_unref(pkt);
        if (ret < 0) {
            if (errorOut) *errorOut = QStringLiteral("av_interleaved_write_frame: ") + ffErr(ret);
            cleanup(outFmt, pkt, false);
            return false;
        }
    }

    ret = av_write_trailer(outFmt);
    cleanup(outFmt, pkt, false);
    if (ret < 0) {
        if (errorOut) *errorOut = QStringLiteral("av_write_trailer: ") + ffErr(ret);
        return false;
    }
    return true;
}

} // namespace

bool AudioMetadataWriter::writeMetadataAndCover(const QString& srcAudioPath,
                                                const QString& dstAudioPath,
                                                const TrackInfo& meta,
                                                const QByteArray& coverBytes,
                                                const QString& embeddedLyrics,
                                                QString* errorOut) {
    if (srcAudioPath.isEmpty() || dstAudioPath.isEmpty() || !QFileInfo::exists(srcAudioPath)) {
        if (errorOut) *errorOut = QStringLiteral("Source audio file missing");
        return false;
    }

    const QFileInfo dstFi(dstAudioPath);
    QDir().mkpath(dstFi.absolutePath());
    const QString ext = dstFi.suffix().toLower();
    const QString muxer = muxerNameForExtension(ext);
    const QString tmpPath = dstFi.dir().filePath(
        QStringLiteral(".virela_dl_%1.%2")
            .arg(QUuid::createUuid().toString(QUuid::Id128))
            .arg(ext.isEmpty() ? QStringLiteral("mp3") : ext));

    auto commitTmp = [&]() -> bool {
        if (QFileInfo::exists(dstAudioPath)) {
            QFile::remove(dstAudioPath);
        }
        if (QFile::rename(tmpPath, dstAudioPath)) {
            return true;
        }
        QFile::remove(tmpPath);
        return false;
    };

    if (!muxer.isEmpty()) {
        if (!coverBytes.isEmpty() && supportsAttachedPic(muxer)) {
            if (remuxOnce(srcAudioPath, tmpPath, muxer, meta, coverBytes, embeddedLyrics, true, errorOut)) {
                if (commitTmp()) return true;
            }
            QFile::remove(tmpPath);
        }
        if (remuxOnce(srcAudioPath, tmpPath, muxer, meta, {}, embeddedLyrics, false, errorOut)) {
            if (commitTmp()) return true;
        }
        QFile::remove(tmpPath);
    }

    // Last-resort lossless file copy fallback so downloads never fail on exotic containers
    if (QFile::copy(srcAudioPath, tmpPath) && commitTmp()) {
        return true;
    }
    QFile::remove(tmpPath);
    if (errorOut && errorOut->isEmpty()) {
        *errorOut = QStringLiteral("Failed to write downloaded file");
    }
    return false;
}

} // namespace virela
