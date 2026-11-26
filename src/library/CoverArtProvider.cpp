#include "library/CoverArtProvider.h"
#include "audio/FFmpegDecoder.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QRunnable>
#include <QSaveFile>
#include <QUrlQuery>
#include <algorithm>

namespace sonora {

namespace {

class CoverImageResponse final : public QQuickImageResponse {
public:
    CoverImageResponse(CoverArtProvider* provider, const QString& hash, int size, QThreadPool* pool)
        : provider_(provider), hash_(hash), size_(size) {
        auto* task = QRunnable::create([this]() {
            if (!hash_.isEmpty() && provider_) {
                image_ = provider_->loadCover(hash_, size_);
            }
            emit finished();
        });
        task->setAutoDelete(true);
        pool->start(task);
    }

    QQuickTextureFactory* textureFactory() const override {
        return image_.isNull() ? nullptr : QQuickTextureFactory::textureFactoryForImage(image_);
    }

private:
    CoverArtProvider* provider_ = nullptr;
    QString hash_;
    int size_ = 256;
    QImage image_;
};

} // namespace

QStringList CoverArtProvider::folderCoverPaths(const QString& audioFilePath) {
    if (audioFilePath.isEmpty()) return {};
    const QFileInfo fi(audioFilePath);
    const QDir dir = fi.dir();
    static const QStringList kCoverNames = {
        QStringLiteral("cover.jpg"),  QStringLiteral("cover.png"),  QStringLiteral("cover.webp"),
        QStringLiteral("folder.jpg"), QStringLiteral("folder.png"), QStringLiteral("folder.webp"),
        QStringLiteral("front.jpg"),  QStringLiteral("front.png"),
        QStringLiteral("album.jpg"),  QStringLiteral("album.png")
    };
    QStringList candidates;
    for (const QString& name : kCoverNames) {
        const QString path = dir.filePath(name);
        if (QFileInfo::exists(path)) candidates.append(path);
    }
    for (const QString& suffix : {QStringLiteral(".jpg"), QStringLiteral(".png")}) {
        const QString path = dir.filePath(fi.completeBaseName() + suffix);
        if (QFileInfo::exists(path) && !candidates.contains(path)) candidates.append(path);
    }
    return candidates;
}

CoverArtProvider::CoverArtProvider(const QString& cacheDir, int maxRamBytes)
    : cacheDir_(cacheDir)
    , maxRamBytes_(std::max(256 * 1024, maxRamBytes)) {
    QDir().mkpath(cacheDir_);
    ramCache_.setMaxCost(maxRamBytes_);
    // Cap cover decode worker threads to 2 to avoid memory spikes during fast scrolling
    ioPool_.setMaxThreadCount(2);
}

QQuickImageResponse* CoverArtProvider::requestImageResponse(const QString& id, const QSize& requestedSize) {
    QString hash = id;
    int targetSize = requestedSize.isValid() ? std::max(requestedSize.width(), requestedSize.height()) : 256;

    const int qIdx = id.indexOf(QLatin1Char('?'));
    if (qIdx >= 0) {
        hash = id.left(qIdx);
        const QUrlQuery query(id.mid(qIdx + 1));
        bool ok = false;
        const int qs = query.queryItemValue(QStringLiteral("size")).toInt(&ok);
        if (ok && qs > 0) {
            targetSize = qs;
        }
    }

    // Bucket sizes to avoid storing 5 slightly different sizes of the same album cover
    if (targetSize <= 112) targetSize = 96;
    else if (targetSize <= 220) targetSize = 200;
    else targetSize = 360;

    return new CoverImageResponse(this, hash, targetSize, &ioPool_);
}

QString CoverArtProvider::diskPathForHash(const QString& hash) const {
    return QDir(cacheDir_).filePath(hash + QStringLiteral(".jpg"));
}

QString CoverArtProvider::storeRawImageBytes(const QByteArray& imageBytes) {
    if (imageBytes.isEmpty()) return {};

    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(imageBytes, QCryptographicHash::Sha1).toHex().left(16));
    const QString dstPath = diskPathForHash(hash);

    if (QFileInfo::exists(dstPath) && !loadCover(hash, 96).isNull()) return hash;

    QBuffer buf;
    buf.setData(imageBytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    reader.setAutoTransform(true);
    const QSize orig = reader.size();
    if (orig.isValid() && (orig.width() > 420 || orig.height() > 420)) {
        reader.setScaledSize(orig.scaled(420, 420, Qt::KeepAspectRatio));
    }
    const QImage img = reader.read();
    if (img.isNull()) return {};

    QDir().mkpath(cacheDir_);
    QSaveFile cachedFile(dstPath);
    if (!cachedFile.open(QIODevice::WriteOnly) || !img.save(&cachedFile, "JPG", 84)
        || !cachedFile.commit()) {
        return {};
    }
    return hash;
}

QString CoverArtProvider::indexCoverForAudioFile(const QString& audioFilePath) {
    if (audioFilePath.isEmpty()) return {};

    // Priority 1: embedded cover picture in audio container
    const QByteArray embedded = FFmpegDecoder::extractEmbeddedCover(audioFilePath);
    if (!embedded.isEmpty()) {
        const QString h = storeRawImageBytes(embedded);
        if (!h.isEmpty()) return h;
    }

    // Priority 2: folder cover image (cover.jpg, folder.png, front.jpg, etc.)
    for (const QString& candidate : folderCoverPaths(audioFilePath)) {
        QFile f(candidate);
        if (f.open(QIODevice::ReadOnly)) {
            const QString hash = storeRawImageBytes(f.readAll());
            if (!hash.isEmpty()) return hash;
        }
    }
    return {};
}

QImage CoverArtProvider::loadFromDiskScaled(const QString& hash, int targetSize) const {
    const QString path = diskPathForHash(hash);
    if (!QFileInfo::exists(path)) {
        return {};
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize orig = reader.size();
    if (orig.isValid() && targetSize > 0 && (orig.width() > targetSize || orig.height() > targetSize)) {
        reader.setScaledSize(orig.scaled(targetSize, targetSize, Qt::KeepAspectRatio));
    }
    return reader.read();
}

QImage CoverArtProvider::loadCover(const QString& coverHash, int targetSize) {
    if (coverHash.isEmpty()) return {};
    const int clampedSize = std::clamp(targetSize, 32, 420);
    const QString key = QStringLiteral("%1_%2").arg(coverHash).arg(clampedSize);

    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        if (QImage* cached = ramCache_.object(key)) {
            return *cached;
        }
    }

    QImage img = loadFromDiskScaled(coverHash, clampedSize);
    if (img.isNull()) {
        return {};
    }

    const int cost = std::max<int>(1, static_cast<int>(img.sizeInBytes()));
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        ramCache_.insert(key, new QImage(img), cost);
    }
    return img;
}

int CoverArtProvider::currentRamCacheBytes() const {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    return ramCache_.totalCost();
}

void CoverArtProvider::clearRamCache() {
    std::lock_guard<std::mutex> lock(cacheMutex_);
    ramCache_.clear();
}

} // namespace sonora
