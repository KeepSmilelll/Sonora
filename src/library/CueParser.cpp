#include "library/CueParser.h"
#include "audio/FFmpegDecoder.h"
#include "core/PlaybackController.h"
#include "core/TextEncodingHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace sonora {

namespace {

QString stripQuotes(const QString& raw) {
    QString s = raw.trimmed();
    if (s.size() >= 2 && s.front() == QLatin1Char('"') && s.back() == QLatin1Char('"')) {
        return s.mid(1, s.size() - 2);
    }
    return s;
}

QString resolveBackingAudioFile(const QString& baseDir, const QString& referencedName) {
    if (baseDir.isEmpty() || referencedName.isEmpty()) {
        return {};
    }
    const QDir dir(baseDir);
    const QString direct = dir.filePath(referencedName);
    if (QFileInfo::exists(direct)) {
        return QFileInfo(direct).absoluteFilePath();
    }

    // Real-world rescue: many CUE sheets reference "CDImage.wav" after conversion to .flac/.ape/.wv
    const QString refStem = QFileInfo(referencedName).completeBaseName();
    const QFileInfoList entries = dir.entryInfoList(QDir::Files);
    QString soleAudioCandidate;
    int audioCount = 0;

    for (const QFileInfo& fi : entries) {
        const QString ext = fi.suffix().toLower();
        if (!PlaybackController::isSupportedAudioExtension(ext)) {
            continue;
        }
        ++audioCount;
        soleAudioCandidate = fi.absoluteFilePath();
        if (fi.completeBaseName().compare(refStem, Qt::CaseInsensitive) == 0) {
            return fi.absoluteFilePath();
        }
    }

    // If directory contains exactly one audio image file, associate it cleanly
    if (audioCount == 1) {
        return soleAudioCandidate;
    }
    return direct;
}

} // namespace

int64_t CueParser::parseCueTimestampMs(const QString& ts) {
    const QStringList parts = ts.trimmed().split(QLatin1Char(':'));
    if (parts.size() != 3) return -1;
    bool okM = false, okS = false, okF = false;
    const int64_t mins = parts[0].toLongLong(&okM);
    const int64_t secs = parts[1].toLongLong(&okS);
    const int64_t frames = parts[2].toLongLong(&okF);
    if (!okM || !okS || !okF || mins < 0 || secs < 0 || frames < 0) return -1;
    return (mins * 60 + secs) * 1000 + (frames * 1000) / 75;
}

CueParseResult CueParser::parseFile(const QString& cueFilePath) {
    CueParseResult res;
    res.cuePath = QFileInfo(cueFilePath).absoluteFilePath();

    QFile f(res.cuePath);
    if (!f.open(QIODevice::ReadOnly)) {
        return res;
    }
    const QByteArray raw = f.readAll();
    const QString text = TextEncodingHelper::decodeBytes(raw);
    const QString baseDir = QFileInfo(res.cuePath).absolutePath();
    return parseText(text, res.cuePath, baseDir, 0);
}

CueParseResult CueParser::parseText(const QString& content,
                                    const QString& cueFilePath,
                                    const QString& baseDir,
                                    int64_t fallbackBackingDurationMs) {
    CueParseResult res;
    res.cuePath = cueFilePath;

    QString albumTitle;
    QString albumArtist;
    QString genre;
    int year = 0;

    QString currentFileRef;
    QString currentResolvedFile;

    struct RawCueTrack {
        int trackNumber = 0;
        QString title;
        QString performer;
        QString backingFile;
        int64_t index01Ms = -1;
        int64_t index00Ms = -1;
    };

    QVector<RawCueTrack> rawTracks;
    RawCueTrack* currentTrack = nullptr;

    static const QRegularExpression kFileRe(QStringLiteral("^FILE\\s+\"([^\"]+)\"\\s+\\S+"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kFileBareRe(QStringLiteral("^FILE\\s+(\\S+)\\s+\\S+"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kTrackRe(QStringLiteral("^TRACK\\s+(\\d+)\\s+AUDIO"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kIndexRe(QStringLiteral("^INDEX\\s+(\\d+)\\s+(\\d+:\\d+:\\d+)"), QRegularExpression::CaseInsensitiveOption);

    const QStringList lines = content.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QStringLiteral("REM "), Qt::CaseInsensitive)) {
            const QString rest = line.mid(4).trimmed();
            if (rest.startsWith(QStringLiteral("GENRE "), Qt::CaseInsensitive)) {
                genre = stripQuotes(rest.mid(6));
            } else if (rest.startsWith(QStringLiteral("DATE "), Qt::CaseInsensitive)) {
                bool ok = false;
                int y = stripQuotes(rest.mid(5)).left(4).toInt(&ok);
                if (ok) year = y;
            }
            continue;
        }

        if (auto m = kFileRe.match(line); m.hasMatch()) {
            currentFileRef = m.captured(1);
            currentResolvedFile = resolveBackingAudioFile(baseDir, currentFileRef);
            currentTrack = nullptr;
            continue;
        }
        if (auto m = kFileBareRe.match(line); m.hasMatch()) {
            currentFileRef = m.captured(1);
            currentResolvedFile = resolveBackingAudioFile(baseDir, currentFileRef);
            currentTrack = nullptr;
            continue;
        }

        if (auto m = kTrackRe.match(line); m.hasMatch()) {
            RawCueTrack t;
            t.trackNumber = m.captured(1).toInt();
            t.backingFile = currentResolvedFile;
            rawTracks.push_back(std::move(t));
            currentTrack = &rawTracks.last();
            continue;
        }

        if (line.startsWith(QStringLiteral("TITLE "), Qt::CaseInsensitive)) {
            const QString val = stripQuotes(line.mid(6));
            if (currentTrack) {
                currentTrack->title = val;
            } else {
                albumTitle = val;
            }
            continue;
        }

        if (line.startsWith(QStringLiteral("PERFORMER "), Qt::CaseInsensitive)) {
            const QString val = stripQuotes(line.mid(10));
            if (currentTrack) {
                currentTrack->performer = val;
            } else {
                albumArtist = val;
            }
            continue;
        }

        if (currentTrack) {
            if (auto m = kIndexRe.match(line); m.hasMatch()) {
                const int idxNum = m.captured(1).toInt();
                const int64_t ms = parseCueTimestampMs(m.captured(2));
                if (ms >= 0) {
                    if (idxNum == 1) {
                        currentTrack->index01Ms = ms;
                    } else if (idxNum == 0) {
                        currentTrack->index00Ms = ms;
                    }
                }
            }
        }
    }

    if (rawTracks.isEmpty()) {
        return res;
    }

    // Probe each distinct backing file once to populate codec, sampleRate, bitDepth, duration
    QHash<QString, TrackInfo> probedBacking;
    for (const auto& rt : rawTracks) {
        if (rt.backingFile.isEmpty() || probedBacking.contains(rt.backingFile)) {
            continue;
        }
        TrackInfo info;
        if (QFileInfo::exists(rt.backingFile) && FFmpegDecoder::probeFile(rt.backingFile, info)) {
            probedBacking.insert(rt.backingFile, info);
            res.referencedBackingFiles.insert(QFileInfo(rt.backingFile).absoluteFilePath());
        } else {
            info.filePath = rt.backingFile;
            info.durationMs = fallbackBackingDurationMs;
            probedBacking.insert(rt.backingFile, info);
            if (!rt.backingFile.isEmpty()) {
                res.referencedBackingFiles.insert(rt.backingFile);
            }
        }
    }

    for (int i = 0; i < rawTracks.size(); ++i) {
        const auto& rt = rawTracks[i];
        if (rt.backingFile.isEmpty() || rt.index01Ms < 0) {
            continue;
        }

        const TrackInfo base = probedBacking.value(rt.backingFile);
        TrackInfo tr = base;
        tr.isCue = true;
        tr.cuePath = cueFilePath;
        tr.filePath = rt.backingFile;
        tr.trackNumber = rt.trackNumber;
        tr.title = rt.title.isEmpty()
            ? QStringLiteral("Track %1").arg(rt.trackNumber, 2, 10, QLatin1Char('0'))
            : rt.title;
        tr.artist = rt.performer.isEmpty() ? albumArtist : rt.performer;
        tr.album = albumTitle.isEmpty() ? base.album : albumTitle;
        tr.albumArtist = albumArtist.isEmpty() ? tr.artist : albumArtist;
        if (!genre.isEmpty()) tr.genre = genre;
        if (year > 0) tr.year = year;

        tr.startMs = rt.index01Ms;

        // End boundary is next track's INDEX 01 if sharing the same backing file, else backing file duration
        int64_t endMs = 0;
        if (i + 1 < rawTracks.size() && rawTracks[i + 1].backingFile == rt.backingFile && rawTracks[i + 1].index01Ms > tr.startMs) {
            endMs = rawTracks[i + 1].index01Ms;
        } else if (base.durationMs > tr.startMs) {
            endMs = base.durationMs;
        }

        tr.endMs = endMs;
        tr.durationMs = endMs > tr.startMs ? (endMs - tr.startMs) : 0;
        res.tracks.push_back(std::move(tr));
    }

    res.valid = !res.tracks.isEmpty();
    return res;
}

} // namespace sonora
