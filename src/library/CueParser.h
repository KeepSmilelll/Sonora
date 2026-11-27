#pragma once

#include "audio/AudioTypes.h"

#include <QSet>
#include <QString>
#include <QVector>

namespace virela {

struct CueParseResult {
    bool valid = false;
    QString cuePath;
    QVector<TrackInfo> tracks;
    QSet<QString> referencedBackingFiles; // canonical paths of backing audio files folded by this CUE
};

class CueParser {
public:
    // Parses a .cue file on disk (auto-detecting UTF-8/GB18030/Shift-JIS) and resolves
    // its backing audio file(s) and subtrack [startMs, endMs] windows.
    static CueParseResult parseFile(const QString& cueFilePath);

    // Pure text parser (exposed for deterministic unit tests without filesystem audio files)
    static CueParseResult parseText(const QString& content,
                                    const QString& cueFilePath,
                                    const QString& baseDir,
                                    int64_t fallbackBackingDurationMs = 0);

    // Converts CUE MM:SS:FF timestamp (75 CD frames per second) to milliseconds.
    static int64_t parseCueTimestampMs(const QString& ts);
};

} // namespace virela
