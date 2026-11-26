#include "lyrics/LyricsParser.h"
#include "core/TextEncodingHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace sonora {

namespace {

enum class InlineTiming { None, ZeroMarkers, TimedWords };

struct TimedLyric {
    LyricLine line;
    InlineTiming timing;
};

qsizetype endTimedTranslationRun(const QVector<TimedLyric>& lines, qsizetype start) {
    if (start + 3 >= lines.size()) return -1;
    const auto& opening = lines[start];
    const auto& first = lines[start + 1];
    if (opening.timing != InlineTiming::ZeroMarkers || !opening.line.text.isEmpty()
        || first.timing != InlineTiming::TimedWords || first.line.text.isEmpty()
        || opening.line.timestampMs != first.line.timestampMs
        || (start > 0 && lines[start - 1].line.timestampMs == opening.line.timestampMs)) {
        return -1;
    }

    // A translation at the end of an original line shares the next line's start timestamp.
    for (qsizetype index = start + 2; index + 1 < lines.size(); index += 2) {
        const auto& translation = lines[index];
        const auto& following = lines[index + 1];
        if (translation.timing != InlineTiming::ZeroMarkers || translation.line.text.isEmpty()
            || translation.line.timestampMs <= lines[index - 1].line.timestampMs
            || translation.line.timestampMs != following.line.timestampMs) {
            break;
        }
        if (following.line.text.isEmpty()) {
            // Require at least one intervening translation/original pair and a closing blank.
            const bool lastAtTimestamp = index + 2 == lines.size()
                || lines[index + 2].line.timestampMs != following.line.timestampMs;
            return index > start + 2 && lastAtTimestamp ? index : -1;
        }
        if (following.timing != InlineTiming::TimedWords) break;
    }
    return -1;
}

int64_t parseTimeTagMs(const QString& minStr, const QString& secStr, const QString& fracStr) {
    const int64_t mins = minStr.toLongLong();
    const int64_t secs = secStr.toLongLong();
    int64_t ms = 0;
    if (!fracStr.isEmpty()) {
        if (fracStr.size() == 1) ms = fracStr.toLongLong() * 100;
        else if (fracStr.size() == 2) ms = fracStr.toLongLong() * 10;
        else ms = fracStr.left(3).toLongLong();
    }
    return (mins * 60 + secs) * 1000 + ms;
}

QVector<LyricWordToken> parseEnhancedWords(const QString& body, QString& cleanTextOut) {
    static const QRegularExpression kWordTag(QStringLiteral("<(\\d{1,3}):(\\d{2})(?:[.:](\\d{1,3}))?>"));
    QVector<LyricWordToken> tokens;
    auto it = kWordTag.globalMatch(body);
    if (!it.hasNext()) {
        cleanTextOut = body.trimmed();
        return tokens;
    }

    QString combined;
    QVector<QRegularExpressionMatch> matches;
    while (it.hasNext()) {
        matches.push_back(it.next());
    }

    for (int i = 0; i < matches.size(); ++i) {
        const auto& m = matches[i];
        const int startPos = m.capturedEnd();
        const int endPos = (i + 1 < matches.size()) ? matches[i + 1].capturedStart() : body.size();
        const QString seg = body.mid(startPos, endPos - startPos);
        LyricWordToken tok;
        tok.timestampMs = parseTimeTagMs(m.captured(1), m.captured(2), m.captured(3));
        tok.text = seg;
        combined.append(seg);
        tokens.push_back(std::move(tok));
    }

    cleanTextOut = combined.trimmed();
    return tokens;
}

} // namespace

ParsedLyrics LyricsParser::parse(const QString& rawText, const QString& sourceLabel) {
    ParsedLyrics result;
    result.sourceLabel = sourceLabel;
    if (rawText.trimmed().isEmpty()) {
        return result;
    }

    // Some LRC files carry separate numeric word markers inside each timed line.
    static const QRegularExpression kInlineWordMarker(QStringLiteral("<([+-]?\\d+)\\s*,\\s*([+-]?\\d+)>"));
    static const QRegularExpression kLineTagRe(QStringLiteral("\\[(\\d{1,3}):(\\d{2})(?:[.:](\\d{1,3}))?\\]"));
    static const QRegularExpression kOffsetRe(QStringLiteral("^\\[offset:\\s*([+-]?\\d+)\\s*\\]$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kMetaTagRe(QStringLiteral("^\\[(ti|ar|al|by|re|ve|length):.*\\]$"), QRegularExpression::CaseInsensitiveOption);

    QVector<TimedLyric> timedLines;
    QVector<LyricLine> plainLines;

    const QStringList rows = rawText.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString& rawRow : rows) {
        QString row = rawRow.trimmed();
        InlineTiming timing = InlineTiming::None;
        auto markers = kInlineWordMarker.globalMatch(row);
        while (markers.hasNext()) {
            const auto marker = markers.next();
            bool firstOk = false;
            bool secondOk = false;
            const auto first = marker.captured(1).toLongLong(&firstOk);
            const auto second = marker.captured(2).toLongLong(&secondOk);
            if (!firstOk || !secondOk || first != 0 || second != 0) {
                timing = InlineTiming::TimedWords;
                break;
            }
            timing = InlineTiming::ZeroMarkers;
        }
        if (timing != InlineTiming::None) row = row.remove(kInlineWordMarker).trimmed();
        if (row.isEmpty()) continue;

        if (auto om = kOffsetRe.match(row); om.hasMatch()) {
            result.offsetMs = om.captured(1).toLongLong();
            continue;
        }
        if (kMetaTagRe.match(row).hasMatch()) {
            continue;
        }

        QVector<int64_t> timestamps;
        int lastTagEnd = 0;
        auto it = kLineTagRe.globalMatch(row);
        while (it.hasNext()) {
            const auto m = it.next();
            timestamps.push_back(parseTimeTagMs(m.captured(1), m.captured(2), m.captured(3)));
            lastTagEnd = m.capturedEnd();
        }

        if (!timestamps.isEmpty()) {
            const QString body = row.mid(lastTagEnd).trimmed();
            QString cleanText;
            const auto words = parseEnhancedWords(body, cleanText);
            for (int64_t ts : timestamps) {
                LyricLine line;
                line.timestampMs = ts;
                line.text = cleanText;
                line.words = words;
                timedLines.push_back({std::move(line), timing});
            }
        } else {
            LyricLine plain;
            plain.timestampMs = -1;
            plain.text = row;
            plainLines.push_back(std::move(plain));
        }
    }

    if (!timedLines.isEmpty()) {
        result.synced = true;
        // Stable sort preserves original order so dual-language lines sharing a timestamp stay paired
        std::stable_sort(timedLines.begin(), timedLines.end(), [](const TimedLyric& a, const TimedLyric& b) {
            return a.line.timestampMs < b.line.timestampMs;
        });

        for (qsizetype index = 0; index < timedLines.size();) {
            const qsizetype end = endTimedTranslationRun(timedLines, index);
            if (end >= 0) {
                for (qsizetype original = index + 1; original < end; original += 2) {
                    auto& line = timedLines[original].line;
                    line.translation = std::move(timedLines[original + 1].line.text);
                    result.lines.push_back(std::move(line));
                }
                index = end + 1;
                continue;
            }

            // Ordinary LRC translations share the original line's start timestamp.
            auto& line = timedLines[index++].line;
            if (!result.lines.isEmpty()
                && result.lines.last().timestampMs == line.timestampMs
                && !line.text.isEmpty()
                && result.lines.last().translation.isEmpty()
                && result.lines.last().text != line.text) {
                if (result.lines.last().text.isEmpty()) {
                    result.lines.last().text = std::move(line.text);
                    result.lines.last().words = std::move(line.words);
                } else {
                    result.lines.last().translation = std::move(line.text);
                }
            } else if (!line.text.isEmpty() || result.lines.isEmpty() || !result.lines.last().text.isEmpty()) {
                result.lines.push_back(std::move(line));
            }
        }
    } else if (!plainLines.isEmpty()) {
        result.synced = false;
        result.lines = std::move(plainLines);
    }

    return result;
}

QString LyricsParser::findLrcFileForAudio(const QString& audioFilePath) {
    if (audioFilePath.isEmpty()) return {};
    const QFileInfo fi(audioFilePath);
    const QDir dir = fi.dir();
    const QString base = fi.completeBaseName();

    const QString direct = dir.filePath(base + QStringLiteral(".lrc"));
    if (QFileInfo::exists(direct)) {
        return direct;
    }

    // Case-insensitive match (e.g. .LRC)
    const QStringList lrcs = dir.entryList({QStringLiteral("*.lrc"), QStringLiteral("*.LRC")}, QDir::Files);
    for (const QString& name : lrcs) {
        if (QFileInfo(name).completeBaseName().compare(base, Qt::CaseInsensitive) == 0) {
            return dir.filePath(name);
        }
    }
    return {};
}

ParsedLyrics LyricsParser::loadForTrack(const QString& audioFilePath, const QString& embeddedLyrics) {
    const QString lrcPath = findLrcFileForAudio(audioFilePath);
    if (!lrcPath.isEmpty()) {
        QFile f(lrcPath);
        if (f.open(QIODevice::ReadOnly)) {
            const QString decoded = TextEncodingHelper::decodeBytes(f.readAll());
            ParsedLyrics parsed = parse(decoded, QStringLiteral("LRC"));
            if (!parsed.lines.isEmpty()) {
                return parsed;
            }
        }
    }
    if (!embeddedLyrics.trimmed().isEmpty()) {
        return parse(embeddedLyrics, QStringLiteral("Embedded"));
    }
    return {};
}

int LyricsParser::findActiveLineIndex(const ParsedLyrics& lyrics, int64_t positionMs, int64_t userOffsetMs) {
    if (!lyrics.synced || lyrics.lines.isEmpty()) {
        return -1;
    }
    const int64_t effectiveMs = positionMs + lyrics.offsetMs + userOffsetMs;
    auto it = std::upper_bound(
        lyrics.lines.begin(),
        lyrics.lines.end(),
        effectiveMs,
        [](int64_t pos, const LyricLine& line) {
            return pos < line.timestampMs;
        });
    if (it == lyrics.lines.begin()) {
        return -1;
    }
    return static_cast<int>(std::distance(lyrics.lines.begin(), it)) - 1;
}

} // namespace sonora
