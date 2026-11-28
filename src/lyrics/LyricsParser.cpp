#include "lyrics/LyricsParser.h"
#include "core/TextEncodingHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>

namespace virela {

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
        const int64_t tagMs = parseTimeTagMs(m.captured(1), m.captured(2), m.captured(3));
        const int startPos = m.capturedEnd();
        const int endPos = (i + 1 < matches.size()) ? matches[i + 1].capturedStart() : body.size();
        const QString seg = body.mid(startPos, endPos - startPos);

        // Trailing empty Enhanced LRC tag acts as the closing end timestamp for the final word.
        if (seg.isEmpty() || (i == matches.size() - 1 && seg.trimmed().isEmpty())) {
            if (!tokens.isEmpty() && tokens.last().endTimestampMs < 0 && tagMs > tokens.last().timestampMs) {
                tokens.last().endTimestampMs = tagMs;
            }
            continue;
        }

        LyricWordToken tok;
        tok.timestampMs = tagMs;
        tok.text = seg;
        combined.append(seg);
        tokens.push_back(std::move(tok));
    }

    cleanTextOut = combined.trimmed();
    return tokens;
}

QVector<LyricWordToken> parseBracketLineWords(const QString& body, int64_t lineStartMs, QString& cleanTextOut) {
    static const QRegularExpression kParenTag(QStringLiteral("\\((\\d+),(\\d+)(?:,\\d+)?\\)"));
    static const QRegularExpression kAngleTag(QStringLiteral("<(\\d+),(\\d+)(?:,\\d+)?>"));
    static const QRegularExpression kLeadingParen(QStringLiteral("^\\s*\\(\\d+,\\d+(?:,\\d+)?\\)"));

    QVector<LyricWordToken> tokens;
    QString combined;

    // 1. NetEase YRC: (wordStartMs,wordDurMs[,flag])text
    if (kLeadingParen.match(body).hasMatch()) {
        QVector<QRegularExpressionMatch> matches;
        auto it = kParenTag.globalMatch(body);
        while (it.hasNext()) matches.push_back(it.next());

        for (int i = 0; i < matches.size(); ++i) {
            const auto& m = matches[i];
            const int64_t startMs = m.captured(1).toLongLong();
            const int64_t durMs = m.captured(2).toLongLong();
            const int startPos = m.capturedEnd();
            const int endPos = (i + 1 < matches.size()) ? matches[i + 1].capturedStart() : body.size();
            const QString seg = body.mid(startPos, endPos - startPos);
            if (seg.isEmpty()) continue;

            LyricWordToken tok;
            tok.timestampMs = startMs;
            tok.endTimestampMs = startMs + std::max<int64_t>(1, durMs);
            tok.text = seg;
            combined.append(seg);
            tokens.push_back(std::move(tok));
        }
        cleanTextOut = combined.trimmed();
        return tokens;
    }

    // 2. KRC-style: <offsetMs,durMs[,flag]>text
    auto angleIt = kAngleTag.globalMatch(body);
    if (angleIt.hasNext()) {
        QVector<QRegularExpressionMatch> matches;
        while (angleIt.hasNext()) matches.push_back(angleIt.next());

        for (int i = 0; i < matches.size(); ++i) {
            const auto& m = matches[i];
            const int64_t startMs = lineStartMs + m.captured(1).toLongLong();
            const int64_t durMs = m.captured(2).toLongLong();
            const int startPos = m.capturedEnd();
            const int endPos = (i + 1 < matches.size()) ? matches[i + 1].capturedStart() : body.size();
            const QString seg = body.mid(startPos, endPos - startPos);
            if (seg.isEmpty()) continue;

            LyricWordToken tok;
            tok.timestampMs = startMs;
            tok.endTimestampMs = startMs + std::max<int64_t>(1, durMs);
            tok.text = seg;
            combined.append(seg);
            tokens.push_back(std::move(tok));
        }
        cleanTextOut = combined.trimmed();
        return tokens;
    }

    // 3. QQ Music QRC: text(wordStartMs,wordDurMs)
    auto parenIt = kParenTag.globalMatch(body);
    if (parenIt.hasNext()) {
        int cursor = 0;
        while (parenIt.hasNext()) {
            const auto m = parenIt.next();
            const QString seg = body.mid(cursor, m.capturedStart() - cursor);
            cursor = m.capturedEnd();
            if (seg.isEmpty()) continue;

            const int64_t startMs = m.captured(1).toLongLong();
            const int64_t durMs = m.captured(2).toLongLong();
            LyricWordToken tok;
            tok.timestampMs = startMs;
            tok.endTimestampMs = startMs + std::max<int64_t>(1, durMs);
            tok.text = seg;
            combined.append(seg);
            tokens.push_back(std::move(tok));
        }
        cleanTextOut = combined.trimmed();
        return tokens;
    }

    cleanTextOut = body.trimmed();
    return tokens;
}

QString findSidecarForAudio(const QString& audioFilePath, const QString& extLower) {
    if (audioFilePath.isEmpty()) return {};
    const QFileInfo fi(audioFilePath);
    const QDir dir = fi.dir();
    const QString base = fi.completeBaseName();

    const QString direct = dir.filePath(base + QLatin1Char('.') + extLower);
    if (QFileInfo::exists(direct)) {
        return direct;
    }

    const QStringList candidates = dir.entryList(
        {QStringLiteral("*.") + extLower, QStringLiteral("*.") + extLower.toUpper()},
        QDir::Files);
    for (const QString& name : candidates) {
        if (QFileInfo(name).completeBaseName().compare(base, Qt::CaseInsensitive) == 0) {
            return dir.filePath(name);
        }
    }
    return {};
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
    static const QRegularExpression kBracketLineRe(QStringLiteral("^\\[(\\d+),(\\d+)\\](.*)$"));
    static const QRegularExpression kLineTagRe(QStringLiteral("\\[(\\d{1,3}):(\\d{2})(?:[.:](\\d{1,3}))?\\]"));
    static const QRegularExpression kOffsetRe(QStringLiteral("^\\[offset:\\s*([+-]?\\d+)\\s*\\]$"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kMetaTagRe(QStringLiteral("^\\[(ti|ar|al|by|re|ve|length):.*\\]$"), QRegularExpression::CaseInsensitiveOption);

    QVector<TimedLyric> timedLines;
    QVector<LyricLine> plainLines;

    const QStringList rows = rawText.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString& rawRow : rows) {
        QString row = rawRow.trimmed();
        if (row.isEmpty()) continue;
        // Skip NetEase YRC JSON metadata header lines such as {"t":0,"c":[...]}
        if (row.startsWith(QLatin1Char('{')) && row.endsWith(QLatin1Char('}'))) {
            continue;
        }

        if (auto bm = kBracketLineRe.match(row); bm.hasMatch()) {
            const int64_t lineStartMs = bm.captured(1).toLongLong();
            const int64_t lineDurMs = bm.captured(2).toLongLong();
            const QString body = bm.captured(3);
            QString cleanText;
            auto words = parseBracketLineWords(body, lineStartMs, cleanText);

            LyricLine line;
            line.timestampMs = lineStartMs;
            line.durationMs = lineDurMs > 0 ? lineDurMs : -1;
            line.text = std::move(cleanText);
            line.words = std::move(words);
            timedLines.push_back({std::move(line), InlineTiming::None});
            continue;
        }

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
                    result.lines.last().durationMs = line.durationMs;
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

ParsedLyrics LyricsParser::parseWithTranslation(const QString& primaryRaw,
                                                const QString& translationRaw,
                                                const QString& sourceLabel) {
    ParsedLyrics base = parse(primaryRaw, sourceLabel);
    if (translationRaw.trimmed().isEmpty() || !base.synced || base.lines.isEmpty()) {
        return base;
    }

    const ParsedLyrics trans = parse(translationRaw);
    if (!trans.synced || trans.lines.isEmpty()) {
        return base;
    }

    QVector<bool> used(trans.lines.size(), false);

    // Pass 1: exact timestamp match (accounting for each stream's [offset:])
    for (int i = 0; i < base.lines.size(); ++i) {
        LyricLine& line = base.lines[i];
        if (line.timestampMs < 0 || line.text.trimmed().isEmpty() || !line.translation.isEmpty()) {
            continue;
        }
        const int64_t baseMs = line.timestampMs + base.offsetMs;
        for (int j = 0; j < trans.lines.size(); ++j) {
            if (used[j]) continue;
            const LyricLine& tl = trans.lines[j];
            if (tl.timestampMs < 0 || tl.text.trimmed().isEmpty()) continue;
            if (tl.timestampMs + trans.offsetMs == baseMs) {
                line.translation = tl.text;
                used[j] = true;
                break;
            }
        }
    }

    // Pass 2: nearest-neighbor alignment within +/- 250 ms tolerance
    for (int i = 0; i < base.lines.size(); ++i) {
        LyricLine& line = base.lines[i];
        if (line.timestampMs < 0 || line.text.trimmed().isEmpty() || !line.translation.isEmpty()) {
            continue;
        }
        const int64_t baseMs = line.timestampMs + base.offsetMs;
        int bestIdx = -1;
        int64_t bestDiff = 251;
        for (int j = 0; j < trans.lines.size(); ++j) {
            if (used[j]) continue;
            const LyricLine& tl = trans.lines[j];
            if (tl.timestampMs < 0 || tl.text.trimmed().isEmpty()) continue;
            const int64_t diff = std::abs((tl.timestampMs + trans.offsetMs) - baseMs);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestIdx = j;
            }
        }
        if (bestIdx >= 0) {
            line.translation = trans.lines[bestIdx].text;
            used[bestIdx] = true;
        }
    }

    return base;
}

QString LyricsParser::composeSidecarLyrics(const QString& yrcRaw,
                                           const QString& lrcRaw,
                                           const QString& tlyricRaw,
                                           QString* chosenExtOut) {
    const QString trimmedYrc = yrcRaw.trimmed();
    const QString trimmedLrc = lrcRaw.trimmed();
    const QString trimmedTrans = tlyricRaw.trimmed();

    QString primary;
    if (!trimmedYrc.isEmpty()) {
        if (chosenExtOut) *chosenExtOut = QStringLiteral("yrc");
        primary = trimmedYrc;
    } else if (!trimmedLrc.isEmpty()) {
        if (chosenExtOut) *chosenExtOut = QStringLiteral("lrc");
        primary = trimmedLrc;
    } else {
        if (chosenExtOut) chosenExtOut->clear();
        return {};
    }

    if (trimmedTrans.isEmpty()) {
        return primary + QLatin1Char('\n');
    }

    const ParsedLyrics merged = parseWithTranslation(primary, trimmedTrans);
    QString out = primary + QLatin1Char('\n');
    for (const LyricLine& line : merged.lines) {
        if (line.timestampMs >= 0 && !line.translation.trimmed().isEmpty()) {
            const int64_t ms = line.timestampMs;
            const int64_t mins = ms / 60000;
            const int64_t secs = (ms / 1000) % 60;
            const int64_t frac = ms % 1000;
            out += QStringLiteral("[%1:%2.%3]%4\n")
                       .arg(mins, 2, 10, QLatin1Char('0'))
                       .arg(secs, 2, 10, QLatin1Char('0'))
                       .arg(frac, 3, 10, QLatin1Char('0'))
                       .arg(line.translation);
        }
    }
    return out;
}

QString LyricsParser::findLrcFileForAudio(const QString& audioFilePath) {
    return findSidecarForAudio(audioFilePath, QStringLiteral("lrc"));
}

ParsedLyrics LyricsParser::loadForTrack(const QString& audioFilePath, const QString& embeddedLyrics) {
    const struct SidecarSpec {
        const char* ext;
        const char* label;
    } sidecars[] = {
        {"yrc", "YRC"},
        {"qrc", "QRC"},
        {"lrc", "LRC"},
    };
    for (const auto& spec : sidecars) {
        const QString path = findSidecarForAudio(audioFilePath, QString::fromLatin1(spec.ext));
        if (!path.isEmpty()) {
            QFile f(path);
            if (f.open(QIODevice::ReadOnly)) {
                const QString decoded = TextEncodingHelper::decodeBytes(f.readAll());
                ParsedLyrics parsed = parse(decoded, QString::fromLatin1(spec.label));
                if (!parsed.lines.isEmpty()) {
                    return parsed;
                }
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

} // namespace virela
