#include "library/M3uPlaylistIO.h"
#include "core/TextEncodingHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

namespace sonora {

QVector<M3uEntry> M3uPlaylistIO::loadFile(const QString& playlistPath) {
    QFile f(playlistPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QString content = TextEncodingHelper::decodeBytes(f.readAll());
    const QString baseDir = QFileInfo(playlistPath).absolutePath();
    return parseText(content, baseDir);
}

QVector<M3uEntry> M3uPlaylistIO::parseText(const QString& content, const QString& baseDir) {
    QVector<M3uEntry> entries;
    const QDir dir(baseDir);

    QString pendingTitle;
    int pendingDuration = -1;

    const QStringList lines = content.split(QRegularExpression(QStringLiteral("\\r?\\n")));
    for (const QString& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) continue;

        if (line.startsWith(QLatin1Char('#'))) {
            if (line.startsWith(QStringLiteral("#EXTINF:"), Qt::CaseInsensitive)) {
                const QString rest = line.mid(8);
                const int comma = rest.indexOf(QLatin1Char(','));
                if (comma >= 0) {
                    bool ok = false;
                    int sec = rest.left(comma).trimmed().toInt(&ok);
                    pendingDuration = ok ? sec : -1;
                    pendingTitle = rest.mid(comma + 1).trimmed();
                }
            }
            continue;
        }

        QString path = line;
        if (path.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive)) {
            path = QUrl(path).toLocalFile();
        }
        path.replace(QLatin1Char('\\'), QLatin1Char('/'));

        QFileInfo fi(path);
        if (fi.isRelative() && !baseDir.isEmpty()) {
            path = QDir::cleanPath(dir.filePath(path));
        } else {
            path = QDir::cleanPath(path);
        }

        M3uEntry e;
        e.filePath = path;
        e.title = pendingTitle;
        e.durationSeconds = pendingDuration;
        entries.push_back(std::move(e));

        pendingTitle.clear();
        pendingDuration = -1;
    }

    return entries;
}

bool M3uPlaylistIO::saveFile(const QString& playlistPath,
                             const QVector<TrackInfo>& tracks,
                             bool preferRelativePaths) {
    QFile f(playlistPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QDir baseDir(QFileInfo(playlistPath).absolutePath());
    QByteArray out = "#EXTM3U\n";
    for (const TrackInfo& t : tracks) {
        const int sec = t.durationMs > 0 ? static_cast<int>(t.durationMs / 1000) : -1;
        const QString label = t.artist.isEmpty()
            ? t.displayTitle()
            : QStringLiteral("%1 - %2").arg(t.artist, t.displayTitle());
        out.append(QStringLiteral("#EXTINF:%1,%2\n").arg(sec).arg(label).toUtf8());

        QString path = t.filePath;
        if (preferRelativePaths) {
            path = baseDir.relativeFilePath(path);
        }
        out.append(path.toUtf8());
        out.append('\n');
    }

    return f.write(out) == out.size();
}

} // namespace sonora
