#include "library/LibraryScanner.h"
#include "audio/FFmpegDecoder.h"
#include "core/PlaybackController.h"
#include "library/CoverArtProvider.h"
#include "library/CueParser.h"
#include "library/LibraryDatabase.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QSet>

namespace virela {

LibraryScanner::LibraryScanner(LibraryDatabase* db, CoverArtProvider* covers, QObject* parent)
    : QObject(parent)
    , db_(db)
    , covers_(covers) {
    debounceTimer_.setSingleShot(true);
    debounceTimer_.setInterval(1500);
    connect(&debounceTimer_, &QTimer::timeout, this, [this]() {
        if (isScanning()) {
            debounceTimer_.start();
        } else if (!watchedFolders_.isEmpty()) {
            startScan(watchedFolders_);
        }
    });
    connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this](const QString&) {
        debounceTimer_.start();
    });
}

LibraryScanner::~LibraryScanner() {
    cancelScan();
}

void LibraryScanner::setWatchedFolders(const QStringList& folders) {
    watchedFolders_ = folders;
    if (!watcher_.directories().isEmpty()) {
        watcher_.removePaths(watcher_.directories());
    }
    for (const QString& f : folders) {
        if (QDir(f).exists()) {
            watcher_.addPath(f);
        }
    }
}

void LibraryScanner::cancelScan() {
    cancel_.store(true, std::memory_order_release);
    if (worker_.joinable()) {
        worker_.join();
    }
    scanning_.store(false, std::memory_order_release);
}

void LibraryScanner::startScan(const QStringList& folders, bool forceRefresh) {
    debounceTimer_.stop();
    cancelScan();
    cancel_.store(false, std::memory_order_release);
    scanning_.store(true, std::memory_order_release);
    scannedCount_.store(0, std::memory_order_release);
    totalCount_.store(0, std::memory_order_release);
    emit scanningChanged();
    emit progressChanged();

    const QStringList foldersCopy = folders;
    worker_ = std::thread([this, foldersCopy, forceRefresh]() {
        const int totalIndexed = runScanWork(foldersCopy, forceRefresh);
        scanning_.store(false, std::memory_order_release);
        QMetaObject::invokeMethod(this, [this, totalIndexed]() {
            emit scanningChanged();
            emit scanFinished(totalIndexed);
        }, Qt::QueuedConnection);
    });
}

int LibraryScanner::scanSynchronously(const QStringList& folders, bool forceRefresh) {
    cancel_.store(false, std::memory_order_release);
    return runScanWork(folders, forceRefresh);
}

int LibraryScanner::runScanWork(const QStringList& folders, bool forceRefresh) {
    if (!db_ || !db_->isOpen()) return 0;

    // 1. Collect all .cue and supported audio files across watched folders
    QStringList cueFiles;
    QStringList audioFiles;
    for (const QString& root : folders) {
        if (cancel_.load(std::memory_order_acquire)) return 0;
        if (!QDir(root).exists()) continue;

        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (cancel_.load(std::memory_order_acquire)) return 0;
            const QString path = QFileInfo(it.next()).absoluteFilePath();
            const QString ext = QFileInfo(path).suffix().toLower();
            if (ext == QStringLiteral("cue")) {
                cueFiles.push_back(path);
            } else if (PlaybackController::isSupportedAudioExtension(ext)) {
                audioFiles.push_back(path);
            }
        }
    }

    const int grandTotal = static_cast<int>(cueFiles.size() + audioFiles.size());
    totalCount_.store(grandTotal, std::memory_order_release);
    QMetaObject::invokeMethod(this, [this]() { emit progressChanged(); }, Qt::QueuedConnection);

    const auto existingIndex = db_->loadFileFingerprintIndex();
    QSet<QString> seenBackingFiles;
    QSet<QString> foldedByCue;
    QHash<QString, QString> dirCoverCache; // Reuse cover hash within same album directory

    QVector<TrackInfo> batch;
    batch.reserve(128);
    int processed = 0;
    int indexedTracks = 0;

    auto flushBatch = [&]() {
        if (!batch.isEmpty()) {
            db_->upsertTracksBatch(batch);
            indexedTracks += static_cast<int>(batch.size());
            batch.clear();
        }
    };

    auto resolveCoverHash = [&](const QString& audioPath) -> QString {
        if (!covers_) return {};
        const QString dirKey = QFileInfo(audioPath).absolutePath();
        // Check embedded cover first; if none, reuse cached folder cover hash
        const QString h = covers_->indexCoverForAudioFile(audioPath);
        if (!h.isEmpty()) {
            if (!dirCoverCache.contains(dirKey)) {
                dirCoverCache.insert(dirKey, h);
            }
            return h;
        }
        return dirCoverCache.value(dirKey);
    };

    // 2. Parse CUE sheets first and record folded backing audio files
    for (const QString& cuePath : cueFiles) {
        if (cancel_.load(std::memory_order_acquire)) break;
        const CueParseResult parsed = CueParser::parseFile(cuePath);
        if (parsed.valid) {
            for (const QString& bf : parsed.referencedBackingFiles) {
                foldedByCue.insert(bf);
                seenBackingFiles.insert(bf);
            }
            QString albumCover;
            if (!parsed.tracks.isEmpty()) {
                albumCover = resolveCoverHash(parsed.tracks.first().filePath);
            }
            for (TrackInfo t : parsed.tracks) {
                t.coverHash = albumCover;
                batch.push_back(std::move(t));
                if (batch.size() >= 128) flushBatch();
            }
        }
        ++processed;
        scannedCount_.store(processed, std::memory_order_release);
    }

    // 3. Index standalone audio files (skip unchanged & skip files folded into CUE sheets)
    for (const QString& audioPath : audioFiles) {
        if (cancel_.load(std::memory_order_acquire)) break;
        if (foldedByCue.contains(audioPath)) {
            ++processed;
            scannedCount_.store(processed, std::memory_order_release);
            continue;
        }

        seenBackingFiles.insert(audioPath);
        const QFileInfo fi(audioPath);
        const int64_t mtime = fi.lastModified().toSecsSinceEpoch();
        const int64_t fsize = fi.size();

        const auto it = existingIndex.constFind(audioPath);
        if (!forceRefresh && it != existingIndex.constEnd()
            && it.value().first == mtime && it.value().second == fsize) {
            ++processed;
            scannedCount_.store(processed, std::memory_order_release);
            continue;
        }

        TrackInfo info;
        if (FFmpegDecoder::probeFile(audioPath, info)) {
            info.coverHash = resolveCoverHash(audioPath);
            batch.push_back(std::move(info));
            if (batch.size() >= 128) {
                flushBatch();
                QMetaObject::invokeMethod(this, [this]() { emit progressChanged(); }, Qt::QueuedConnection);
            }
        }
        ++processed;
        scannedCount_.store(processed, std::memory_order_release);
    }

    flushBatch();

    // 4. Prune removed files that are no longer on disk or outside watched folders
    if (!cancel_.load(std::memory_order_acquire)) {
        db_->removeTracksOutsideFolders(folders);
        for (auto it = existingIndex.constBegin(); it != existingIndex.constEnd(); ++it) {
            if (!seenBackingFiles.contains(it.key()) && !QFileInfo::exists(it.key())) {
                db_->removeTracksForBackingFile(it.key());
            }
        }
    }

    QMetaObject::invokeMethod(this, [this]() { emit progressChanged(); }, Qt::QueuedConnection);
    return indexedTracks;
}

} // namespace virela
