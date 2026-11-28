#pragma once

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QObject>
#include <QString>
#include <QThread>
#include <QUrl>
#include <QVector>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

class QNetworkAccessManager;

namespace virela {

class StreamCacheManager;
class StreamNetworkWorker;

struct BlockingHttpResult {
    bool ok = false;
    bool aborted = false;
    bool timedOut = false;
    int statusCode = 0;
    QByteArray body;
    QUrl redirectLocation;
    QString contentType;
    QString errorString;
};

class StreamCacheSession : public std::enable_shared_from_this<StreamCacheSession> {
public:
    StreamCacheSession(StreamCacheManager* manager,
                       QString cacheKey,
                       QString partPath,
                       QString finalPath,
                       QUrl url,
                       QString cookie,
                       QString extHint);
    ~StreamCacheSession();

    StreamCacheSession(const StreamCacheSession&) = delete;
    StreamCacheSession& operator=(const StreamCacheSession&) = delete;

    const QString& cacheKey() const noexcept { return cacheKey_; }
    QString finalPath() const;
    QString partPath() const { return partPath_; }
    QString ext() const;
    int64_t totalSize() const;
    bool acceptRanges() const;
    bool isComplete() const;
    bool isFailed() const;
    bool isCancelled() const;
    QString errorString() const;
    double progressRatio() const;

    void addReader();
    void releaseReader();
    int readerCount() const noexcept { return readerCount_.load(std::memory_order_acquire); }

    void setDownloadPinned(bool pinned);
    bool isDownloadPinned() const noexcept { return downloadPinned_.load(std::memory_order_acquire); }

    // Waits until response headers/totalSize/ext are known (or stream completes/fails/aborts).
    bool waitForHeaders(int timeoutMs, const std::atomic<bool>* abortFlag = nullptr, bool* timedOutOut = nullptr);

    // Reads up to maxLen bytes at byte offset, blocking until data arrives, EOF, abort, or timeout.
    // Returns >0 on data, 0 on EOF, -1 on error/abort/timeout.
    int readAt(int64_t offset,
               uint8_t* dst,
               int maxLen,
               int timeoutMs,
               const std::atomic<bool>* abortFlag = nullptr,
               bool* timedOutOut = nullptr,
               bool* abortedOut = nullptr);

    // Returns first missing [start, end) range when totalSize_ > 0, or {-1, -1} if complete.
    std::pair<int64_t, int64_t> firstMissingGap() const;

    void cancel();

private:
    friend class StreamCacheManager;
    friend class StreamNetworkWorker;

    void markStarted();
    void onHeadersReceived(int statusCode,
                           int64_t totalSize,
                           bool acceptRanges,
                           const QString& detectedExt,
                           int64_t activeStartOffset,
                           quint64 generation);
    void onChunkReceived(int64_t writeOffset, const QByteArray& chunk, quint64 generation);
    void onTransferFinished(bool ok, int httpStatus, const QString& errMsg, quint64 generation);
    bool finalizeIfCompleteLocked(std::unique_lock<std::mutex>& stateLock);
    void mergeRangeLocked(int64_t start, int64_t end);
    int64_t contiguousEndFromLocked(int64_t offset) const;
    std::pair<int64_t, int64_t> firstMissingGapLocked() const;

    StreamCacheManager* manager_ = nullptr;
    QString cacheKey_;
    QString partPath_;
    QString finalPath_;
    QString activePath_;
    QUrl url_;
    QString cookie_;
    QString ext_;

    mutable std::mutex stateMutex_;
    std::condition_variable stateCv_;

    std::mutex fileMutex_;
    QFile file_;

    int64_t totalSize_ = -1;
    int64_t currentWriteOffset_ = 0;
    int64_t activeNetBaseOffset_ = 0;
    int64_t requestedJumpOffset_ = -1;
    quint64 netGeneration_ = 0;
    bool acceptRanges_ = false;
    bool headersReady_ = false;
    bool complete_ = false;
    bool failed_ = false;
    QString errorString_;
    QVector<std::pair<int64_t, int64_t>> downloadedRanges_;

    std::atomic<int> readerCount_{0};
    std::atomic<bool> downloadPinned_{false};
    std::atomic<bool> cancelled_{false};
};

class StreamCacheManager : public QObject {
    Q_OBJECT

    Q_PROPERTY(qint64 currentCacheBytes READ currentCacheBytes NOTIFY cacheStatsChanged)
    Q_PROPERTY(qint64 maxCacheBytes READ maxCacheBytes NOTIFY cacheStatsChanged)

public:
    static constexpr qint64 kDefaultMaxCacheBytes = 1024LL * 1024 * 1024; // 1 GB

    explicit StreamCacheManager(const QString& cacheDir,
                                qint64 maxCacheBytes = kDefaultMaxCacheBytes,
                                QObject* parent = nullptr);
    ~StreamCacheManager() override;

    static QString makeCacheKey(const QString& source, const QString& remoteId, const QString& quality);
    static QString inferExtension(const QString& contentType, const QUrl& url, const QString& hint = {});

    QString cacheDir() const { return cacheDir_; }
    qint64 maxCacheBytes() const;
    void setMaxCacheBytes(qint64 maxBytes);
    qint64 currentCacheBytes() const;

    // Returns completed cached file path if present on disk (updates LRU timestamp).
    QString findCompletedCacheFile(const QString& cacheKey);

    // Reuses an active session for cacheKey if present (zero duplicate traffic), or starts a new one.
    std::shared_ptr<StreamCacheSession> openOrStartSession(const QString& cacheKey,
                                                           const QUrl& url,
                                                           const QString& cookie = {},
                                                           const QString& extHint = {});

    std::shared_ptr<StreamCacheSession> activeSessionForKey(const QString& cacheKey) const;

    void enforceLruLimit();
    Q_INVOKABLE void clearCache();

    // Thread-safe blocking HTTP GET executed on internal network thread (never blocks UI thread).
    BlockingHttpResult runBlockingHttpGet(const QUrl& url,
                                            const QString& cookie,
                                            int timeoutMs,
                                            const std::atomic<bool>* abortFlag = nullptr,
                                            bool followRedirects = true);

signals:
    void cacheStatsChanged();
    void sessionCompleted(const QString& cacheKey, const QString& finalPath);
    void sessionFailed(const QString& cacheKey, const QString& error);

private:
    friend class StreamCacheSession;
    friend class StreamNetworkWorker;

    struct CacheEntry {
        QString key;
        QString fileName;
        QString ext;
        qint64 sizeBytes = 0;
        qint64 lastAccessMs = 0;
    };

    void loadIndex();
    void saveIndexLocked() const;
    void cleanStalePartFiles();
    void onSessionCompletedInternal(const QString& cacheKey,
                                    const QString& finalPath,
                                    const QString& ext,
                                    qint64 sizeBytes);
    void onSessionClosedOrFailed(const QString& cacheKey);
    void requestSessionRange(const std::shared_ptr<StreamCacheSession>& session,
                             int64_t startOffset,
                             int64_t endOffset,
                             quint64 generation);
    void cancelSessionNetwork(const QString& cacheKey);
    void ensureNetworkThreadStarted();

    QString cacheDir_;
    qint64 maxCacheBytes_ = kDefaultMaxCacheBytes;

    mutable std::mutex indexMutex_;
    QHash<QString, CacheEntry> entries_;

    mutable std::mutex sessionMutex_;
    QHash<QString, std::weak_ptr<StreamCacheSession>> activeSessions_;

    std::mutex workerInitMutex_;
    QThread netThread_;
    StreamNetworkWorker* worker_ = nullptr;
};

} // namespace virela
