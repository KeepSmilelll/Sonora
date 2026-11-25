#pragma once

#include <QString>
#include <QVector>
#include <cstdint>

namespace sonora {

struct LyricWordToken {
    int64_t timestampMs = 0;
    QString text;
};

struct LyricLine {
    int64_t timestampMs = -1; // -1 for unsynced plain lyrics
    QString text;
    QString translation;
    QVector<LyricWordToken> words; // populated for Enhanced LRC <mm:ss.xx>
};

struct ParsedLyrics {
    bool synced = false;
    int64_t offsetMs = 0;
    QVector<LyricLine> lines;
    QString sourceLabel; // e.g. "LRC File" or "Embedded"
};

class LyricsParser {
public:
    static ParsedLyrics parse(const QString& rawText, const QString& sourceLabel = {});

    // Locates a matching .lrc file alongside the given audio file and decodes it (UTF-8/UTF-16/GB18030)
    static QString findLrcFileForAudio(const QString& audioFilePath);
    static ParsedLyrics loadForTrack(const QString& audioFilePath, const QString& embeddedLyrics = {});

    // O(log N) lookup of the active lyric line index for a given playback position
    static int findActiveLineIndex(const ParsedLyrics& lyrics, int64_t positionMs, int64_t userOffsetMs = 0);
};

} // namespace sonora
