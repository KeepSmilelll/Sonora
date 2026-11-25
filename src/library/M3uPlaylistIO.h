#pragma once

#include "audio/AudioTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace sonora {

struct M3uEntry {
    QString filePath;
    QString title;
    int durationSeconds = -1;
};

class M3uPlaylistIO {
public:
    // Loads an .m3u / .m3u8 file (auto-detecting UTF-8 / GB18030) and resolves relative paths.
    static QVector<M3uEntry> loadFile(const QString& playlistPath);
    static QVector<M3uEntry> parseText(const QString& content, const QString& baseDir);

    // Exports tracks to UTF-8 #EXTM3U (.m3u8) format.
    static bool saveFile(const QString& playlistPath,
                         const QVector<TrackInfo>& tracks,
                         bool preferRelativePaths = false);
};

} // namespace sonora
