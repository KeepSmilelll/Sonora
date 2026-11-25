#pragma once

#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <cstdint>

namespace sonora {

enum class PlaybackState : int {
    Stopped = 0,
    Playing = 1,
    Paused = 2,
    Buffering = 3
};

enum class RepeatMode : int {
    Off = 0,
    All = 1,
    One = 2
};

enum class OutputMode : int {
    WasapiShared = 0,
    WasapiExclusive = 1
};

enum class ReplayGainMode : int {
    Off = 0,
    Track = 1,
    Album = 2
};

struct TrackInfo {
    int64_t id = -1;
    QString filePath;
    bool isCue = false;
    QString cuePath;
    int64_t startMs = 0;
    int64_t endMs = 0; // 0 means play until EOF

    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    int trackNumber = 0;
    int discNumber = 0;
    int year = 0;
    QString genre;

    int64_t durationMs = 0;
    int sampleRate = 0;
    int bitDepth = 0;
    int bitrateKbps = 0;
    int channels = 0;
    QString codec;
    QString formatBadge; // e.g. "DSD64", "Hi-Res", "Lossless", "MP3", "OPUS"
    bool isDsd = false;

    double rgTrackGainDb = 0.0;
    double rgTrackPeak = 1.0;
    double rgAlbumGainDb = 0.0;
    double rgAlbumPeak = 1.0;
    bool hasRgTrack = false;
    bool hasRgAlbum = false;

    QString coverHash;
    int64_t fileSize = 0;
    int64_t fileMtime = 0;

    bool isFavorite = false;
    int playCount = 0;
    int64_t lastPlayedAt = 0;
    int64_t addedAt = 0;

    QString embeddedLyrics;

    QString displayTitle() const {
        return title.isEmpty() ? filePath : title;
    }

    QString techSummary() const {
        if (codec.isEmpty() || sampleRate <= 0) {
            return {};
        }
        if (isDsd) {
            const double mhz = static_cast<double>(sampleRate) / 1'000'000.0;
            return QStringLiteral("%1 · %2 MHz · 1-bit")
                .arg(formatBadge.isEmpty() ? QStringLiteral("DSD") : formatBadge)
                .arg(mhz, 0, 'f', 2);
        }
        const double khz = static_cast<double>(sampleRate) / 1000.0;
        const QString rateStr = (sampleRate % 1000 == 0)
            ? QString::number(sampleRate / 1000) + QStringLiteral(" kHz")
            : QString::number(khz, 'f', 1) + QStringLiteral(" kHz");
        if (bitDepth > 0) {
            return QStringLiteral("%1 · %2 / %3-bit").arg(codec, rateStr).arg(bitDepth);
        }
        if (bitrateKbps > 0) {
            return QStringLiteral("%1 · %2 · %3 kbps").arg(codec, rateStr).arg(bitrateKbps);
        }
        return QStringLiteral("%1 · %2").arg(codec, rateStr);
    }

    QVariantMap toVariantMap() const {
        return {
            {QStringLiteral("id"), QVariant::fromValue(id)},
            {QStringLiteral("filePath"), filePath},
            {QStringLiteral("isCue"), isCue},
            {QStringLiteral("cuePath"), cuePath},
            {QStringLiteral("startMs"), QVariant::fromValue(startMs)},
            {QStringLiteral("endMs"), QVariant::fromValue(endMs)},
            {QStringLiteral("title"), title},
            {QStringLiteral("artist"), artist},
            {QStringLiteral("album"), album},
            {QStringLiteral("albumArtist"), albumArtist},
            {QStringLiteral("trackNumber"), trackNumber},
            {QStringLiteral("discNumber"), discNumber},
            {QStringLiteral("year"), year},
            {QStringLiteral("genre"), genre},
            {QStringLiteral("durationMs"), QVariant::fromValue(durationMs)},
            {QStringLiteral("sampleRate"), sampleRate},
            {QStringLiteral("bitDepth"), bitDepth},
            {QStringLiteral("bitrateKbps"), bitrateKbps},
            {QStringLiteral("channels"), channels},
            {QStringLiteral("codec"), codec},
            {QStringLiteral("formatBadge"), formatBadge},
            {QStringLiteral("techSummary"), techSummary()},
            {QStringLiteral("isDsd"), isDsd},
            {QStringLiteral("rgTrackGainDb"), rgTrackGainDb},
            {QStringLiteral("rgTrackPeak"), rgTrackPeak},
            {QStringLiteral("rgAlbumGainDb"), rgAlbumGainDb},
            {QStringLiteral("rgAlbumPeak"), rgAlbumPeak},
            {QStringLiteral("hasRgTrack"), hasRgTrack},
            {QStringLiteral("hasRgAlbum"), hasRgAlbum},
            {QStringLiteral("coverHash"), coverHash},
            {QStringLiteral("isFavorite"), isFavorite},
            {QStringLiteral("playCount"), playCount}
        };
    }
};

struct AudioDeviceInfo {
    QString id;
    QString name;
    bool isDefault = false;
};

struct OutputConfig {
    QString deviceId; // empty = system default
    OutputMode mode = OutputMode::WasapiShared;
    bool autoNativeSampleRate = true;
    bool bitPerfectBypass = true;
    int dsdTargetSampleRate = 176400; // 88200 / 176400 / 352800
    int preferredSampleRate = 0;      // 0 = follow track/device
    int channels = 2;
};

struct ActiveOutputStatus {
    int sampleRate = 48000;
    int channels = 2;
    QString hardwareFormat; // e.g. "32-bit Float", "24-bit PCM"
    bool exclusiveActive = false;
    bool bitPerfectActive = false;
    QString deviceName;
    QString fallbackNotice;
};

} // namespace sonora

Q_DECLARE_METATYPE(sonora::TrackInfo)
Q_DECLARE_METATYPE(sonora::AudioDeviceInfo)
Q_DECLARE_METATYPE(sonora::ActiveOutputStatus)
