#pragma once

#include <QCache>
#include <QImage>
#include <QQuickAsyncImageProvider>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <mutex>

namespace virela {

// Asynchronous cover art provider with strict 8 MB RAM LRU cap and disk thumbnail cache.
// URI format: image://cover/<coverHash>?size=96 (or size=360)
class CoverArtProvider : public QQuickAsyncImageProvider {
public:
    static constexpr int kDefaultRamCacheBytes = 8 * 1024 * 1024; // 8 MB strict cap

    explicit CoverArtProvider(const QString& cacheDir, int maxRamBytes = kDefaultRamCacheBytes);
    ~CoverArtProvider() override = default;

    QQuickImageResponse* requestImageResponse(const QString& id, const QSize& requestedSize) override;

    // Extracts (embedded -> folder cover) and caches a 420px JPEG thumbnail on disk.
    // Returns stable 16-char hex coverHash, or empty string if no cover found.
    QString indexCoverForAudioFile(const QString& audioFilePath);

    static QStringList folderCoverPaths(const QString& audioFilePath);

    // Synchronous read from RAM LRU or disk thumbnail cache (used by ThemeManager / SMTC / tests)
    QImage loadCover(const QString& coverHash, int targetSize = 360);

    // Exposes current RAM LRU byte cost for unit tests and memory diagnostics
    int currentRamCacheBytes() const;
    int maxRamCacheBytes() const { return maxRamBytes_; }
    void clearRamCache();

    // Helper for unit tests to inject synthetic artwork directly into disk + RAM cache
    QString storeRawImageBytes(const QByteArray& imageBytes);

private:
    QString diskPathForHash(const QString& hash) const;
    QImage loadFromDiskScaled(const QString& hash, int targetSize) const;

    QString cacheDir_;
    int maxRamBytes_ = kDefaultRamCacheBytes;
    mutable std::mutex cacheMutex_;
    QCache<QString, QImage> ramCache_;
    QThreadPool ioPool_;
};

} // namespace virela
