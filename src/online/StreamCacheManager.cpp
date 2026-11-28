#include "online/StreamCacheManager.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimer>
#include <algorithm>
#include <chrono>

namespace virela {

namespace {

constexpr int kMaxRedirects = 5;

struct ActiveDownloadJob {
    QNetworkReply* reply = nullptr;
    std::weak_ptr<StreamCacheSession> session;
    quint64 generation = 0;
    int64_t requestedStart = 0;
    int64_t writeCursor = 0;
    int64_t expectedTotal = -1;
    bool acceptRanges = false;
    bool headersDispatched = false;
    int redirectCount = 0;
    QUrl currentUrl;
    QString cookie;
};

struct BlockingHttpWait {
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    BlockingHttpResult result;
    std::atomic<bool> abortRequested{false};
};

QString normalizeExt(QString ext) {
    ext = ext.trimmed().toLower();
    if (ext.startsWith(QLatin1Char('.'))) ext = ext.mid(1);
    if (ext == QStringLiteral("mpeg")) return QStringLiteral("mp3");
    if (ext == QStringLiteral("mp4")) return QStringLiteral("m4a");
    return ext;
}

bool parseContentRange(const QByteArray& hdr, int64_t& outStart, int64_t& outEnd, int64_t& outTotal) {
    static const QRegularExpression re(QStringLiteral("^bytes\\s+(\\d+)-(\\d+)/(\\d+|\\*)$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const auto m = re.match(QString::fromLatin1(hdr).trimmed());
    if (!m.hasMatch()) return false;
    outStart = m.captured(1).toLongLong();
    outEnd = m.captured(2).toLongLong();
    const QString tot = m.captured(3);
    outTotal = (tot == QStringLiteral("*")) ? -1 : tot.toLongLong();
    return true;
}

} // namespace

class StreamNetworkWorker final : public QObject {
    Q_OBJECT

public:
    explicit StreamNetworkWorker(QObject* parent = nullptr) : QObject(parent) {}

    ~StreamNetworkWorker() override {
        for (auto it = jobs_.begin(); it != jobs_.end(); ++it) {
            if (it.value().reply) {
                it.value().reply->abort();
                it.value().reply->deleteLater();
            }
        }
        jobs_.clear();
        delete nam_;
        nam_ = nullptr;
    }

public slots:
    void init() {
        if (!nam_) {
            nam_ = new QNetworkAccessManager(this);
        }
    }

    void startRangeTransfer(const QString& cacheKey,
                            const std::shared_ptr<StreamCacheSession>& session,
                            const QUrl& url,
                            const QString& cookie,
                            int64_t startOffset,
                            int64_t endOffset,
                            quint64 generation,
                            int redirectCount) {
        init();
        if (!session || session->isCancelled()) return;

        // Abort previous active reply for this key
        if (jobs_.contains(cacheKey)) {
            auto old = jobs_.take(cacheKey);
            if (old.reply) {
                old.reply->disconnect(this);
                old.reply->abort();
                old.reply->deleteLater();
            }
        }

        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Virela/2.0"));
        if (!cookie.isEmpty()) {
            req.setRawHeader("Cookie", cookie.toUtf8());
        }
        if (startOffset > 0 || endOffset >= startOffset) {
            QByteArray rangeVal = "bytes=" + QByteArray::number(startOffset) + "-";
            if (endOffset >= startOffset) {
                rangeVal += QByteArray::number(endOffset);
            }
            req.setRawHeader("Range", rangeVal);
        }

        QNetworkReply* reply = nam_->get(req);
        ActiveDownloadJob job;
        job.reply = reply;
        job.session = session;
        job.generation = generation;
        job.requestedStart = startOffset;
        job.writeCursor = startOffset;
        job.redirectCount = redirectCount;
        job.currentUrl = url;
        job.cookie = cookie;
        jobs_.insert(cacheKey, job);

        connect(reply, &QNetworkReply::metaDataChanged, this, [this, cacheKey]() {
            onMetaData(cacheKey);
        });
        connect(reply, &QNetworkReply::readyRead, this, [this, cacheKey]() {
            onReadyRead(cacheKey);
        });
        connect(reply, &QNetworkReply::finished, this, [this, cacheKey]() {
            onFinished(cacheKey);
        });
    }

    void cancelTransfer(const QString& cacheKey) {
        if (!jobs_.contains(cacheKey)) return;
        auto job = jobs_.take(cacheKey);
        if (job.reply) {
            job.reply->disconnect(this);
            job.reply->abort();
            job.reply->deleteLater();
        }
    }

    void performBlockingGet(const QUrl& url,
                            const QString& cookie,
                            int timeoutMs,
                            bool followRedirects,
                            int redirectCount,
                            const std::shared_ptr<BlockingHttpWait>& waitState) {
        init();
        if (!waitState || waitState->abortRequested.load(std::memory_order_acquire)) {
            if (waitState) {
                std::lock_guard<std::mutex> lock(waitState->mutex);
                waitState->result.aborted = true;
                waitState->done = true;
                waitState->cv.notify_all();
            }
            return;
        }

        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Virela/2.0"));
        if (!cookie.isEmpty()) {
            req.setRawHeader("Cookie", cookie.toUtf8());
        }

        QNetworkReply* reply = nam_->get(req);
        auto* timer = new QTimer(reply);
        timer->setSingleShot(true);

        auto* pollAbort = new QTimer(reply);
        pollAbort->setInterval(25);
        connect(pollAbort, &QTimer::timeout, reply, [reply, waitState]() {
            if (waitState->abortRequested.load(std::memory_order_acquire)) {
                reply->abort();
            }
        });
        pollAbort->start();

        auto timedOutFlag = std::make_shared<bool>(false);
        connect(timer, &QTimer::timeout, reply, [reply, timedOutFlag]() {
            *timedOutFlag = true;
            reply->abort();
        });
        timer->start(std::max(500, timeoutMs));

        connect(reply, &QNetworkReply::finished, this,
                [this, reply, url, cookie, timeoutMs, followRedirects, redirectCount, waitState, timedOutFlag]() {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QUrl loc = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
            const QUrl resolvedLoc = loc.isValid() ? url.resolved(loc) : QUrl();
            const QString contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();

            if (followRedirects && status >= 300 && status < 400 && resolvedLoc.isValid()
                && redirectCount < kMaxRedirects && !waitState->abortRequested.load(std::memory_order_acquire)) {
                reply->deleteLater();
                const QString nextCookie = (resolvedLoc.host() == url.host()) ? cookie : QString();
                performBlockingGet(resolvedLoc, nextCookie, timeoutMs, followRedirects, redirectCount + 1, waitState);
                return;
            }

            BlockingHttpResult res;
            res.statusCode = status;
            res.redirectLocation = resolvedLoc;
            res.contentType = contentType;
            res.aborted = waitState->abortRequested.load(std::memory_order_acquire);
            res.timedOut = *timedOutFlag;
            if (!res.aborted && !res.timedOut && reply->error() == QNetworkReply::NoError) {
                res.ok = true;
                res.body = reply->readAll();
            } else if (!res.aborted && !res.timedOut && status >= 300 && status < 400 && resolvedLoc.isValid()) {
                res.ok = true;
                res.body = reply->readAll();
            } else {
                res.errorString = res.timedOut
                    ? QStringLiteral("Network request timed out")
                    : reply->errorString();
                res.body = reply->readAll();
            }
            reply->deleteLater();

            {
                std::lock_guard<std::mutex> lock(waitState->mutex);
                waitState->result = std::move(res);
                waitState->done = true;
            }
            waitState->cv.notify_all();
        });
    }

private:
    void onMetaData(const QString& cacheKey) {
        if (!jobs_.contains(cacheKey)) return;
        ActiveDownloadJob& job = jobs_[cacheKey];
        QNetworkReply* reply = job.reply;
        auto session = job.session.lock();
        if (!reply || !session || session->isCancelled()) return;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl redir = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (status >= 300 && status < 400 && redir.isValid()) {
            return; // Wait for finished() to follow redirect cleanly without writing redirect body
        }

        if (job.headersDispatched) return;
        job.headersDispatched = true;

        int64_t totalSize = -1;
        bool acceptRanges = false;
        int64_t activeStart = job.requestedStart;

        const QByteArray cr = reply->rawHeader("Content-Range");
        int64_t crStart = 0, crEnd = 0, crTotal = -1;
        if (status == 206 && parseContentRange(cr, crStart, crEnd, crTotal)) {
            activeStart = crStart;
            job.writeCursor = crStart;
            totalSize = crTotal;
            acceptRanges = true;
        } else {
            const QVariant clVar = reply->header(QNetworkRequest::ContentLengthHeader);
            if (clVar.isValid()) {
                const int64_t cl = clVar.toLongLong();
                if (cl > 0) totalSize = cl;
            }
            const QByteArray ar = reply->rawHeader("Accept-Ranges").trimmed().toLower();
            acceptRanges = (ar == "bytes");
            if (status == 200 && job.requestedStart > 0) {
                // Server ignored Range header and restarted from byte 0
                activeStart = 0;
                job.writeCursor = 0;
                acceptRanges = false;
            }
        }

        const QString ct = reply->header(QNetworkRequest::ContentTypeHeader).toString();
        const QString ext = StreamCacheManager::inferExtension(ct, job.currentUrl);
        job.expectedTotal = totalSize;
        job.acceptRanges = acceptRanges;

        session->onHeadersReceived(status, totalSize, acceptRanges, ext, activeStart, job.generation);
    }

    void onReadyRead(const QString& cacheKey) {
        if (!jobs_.contains(cacheKey)) return;
        ActiveDownloadJob& job = jobs_[cacheKey];
        QNetworkReply* reply = job.reply;
        auto session = job.session.lock();
        if (!reply || !session || session->isCancelled()) return;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status >= 300 && status < 400) {
            reply->readAll();
            return;
        }
        if (!job.headersDispatched) {
            onMetaData(cacheKey);
        }

        const QByteArray data = reply->readAll();
        if (!data.isEmpty()) {
            const int64_t writeOff = job.writeCursor;
            job.writeCursor += data.size();
            session->onChunkReceived(writeOff, data, job.generation);
        }
    }

    void onFinished(const QString& cacheKey) {
        if (!jobs_.contains(cacheKey)) return;
        ActiveDownloadJob job = jobs_.take(cacheKey);
        QNetworkReply* reply = job.reply;
        auto session = job.session.lock();
        if (!reply) return;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QUrl redir = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        const QUrl resolvedRedir = redir.isValid() ? job.currentUrl.resolved(redir) : QUrl();

        if (status >= 300 && status < 400 && resolvedRedir.isValid() && job.redirectCount < kMaxRedirects) {
            reply->deleteLater();
            if (session && !session->isCancelled()) {
                const QString nextCookie = (resolvedRedir.host() == job.currentUrl.host()) ? job.cookie : QString();
                startRangeTransfer(cacheKey, session, resolvedRedir, nextCookie,
                                   job.requestedStart, -1, job.generation, job.redirectCount + 1);
            }
            return;
        }

        if (session && !session->isCancelled()) {
            if (!job.headersDispatched && reply->error() == QNetworkReply::NoError) {
                // Make sure headers get dispatched even for tiny 0-byte or instant responses
                const QVariant clVar = reply->header(QNetworkRequest::ContentLengthHeader);
                const int64_t cl = clVar.isValid() ? clVar.toLongLong() : -1;
                const QString ct = reply->header(QNetworkRequest::ContentTypeHeader).toString();
                const QString ext = StreamCacheManager::inferExtension(ct, job.currentUrl);
                session->onHeadersReceived(status, cl, false, ext, job.writeCursor, job.generation);
            }
            const QByteArray rem = reply->readAll();
            if (!rem.isEmpty()) {
                const int64_t writeOff = job.writeCursor;
                job.writeCursor += rem.size();
                session->onChunkReceived(writeOff, rem, job.generation);
            }

            const bool ok = (reply->error() == QNetworkReply::NoError) && (status == 0 || (status >= 200 && status < 300));
            const QString err = ok ? QString() : reply->errorString();
            session->onTransferFinished(ok, status, err, job.generation);
        }

        reply->deleteLater();
    }

    QNetworkAccessManager* nam_ = nullptr;
    QHash<QString, ActiveDownloadJob> jobs_;
};

// ============================================================================
// StreamCacheSession
// ============================================================================

StreamCacheSession::StreamCacheSession(StreamCacheManager* manager,
                                       QString cacheKey,
                                       QString partPath,
                                       QString finalPath,
                                       QUrl url,
                                       QString cookie,
                                       QString extHint)
    : manager_(manager)
    , cacheKey_(std::move(cacheKey))
    , partPath_(std::move(partPath))
    , finalPath_(std::move(finalPath))
    , activePath_(partPath_)
    , url_(std::move(url))
    , cookie_(std::move(cookie))
    , ext_(normalizeExt(extHint.isEmpty() ? QStringLiteral("mp3") : extHint))
    , file_(partPath_) {
    QDir().mkpath(QFileInfo(partPath_).absolutePath());
    std::lock_guard<std::mutex> flock(fileMutex_);
    if (!file_.open(QIODevice::ReadWrite | QIODevice::Truncate)) {
        std::lock_guard<std::mutex> slock(stateMutex_);
        failed_ = true;
        errorString_ = QStringLiteral("Failed to open cache file: ") + partPath_;
    }
}

StreamCacheSession::~StreamCacheSession() {
    {
        std::lock_guard<std::mutex> flock(fileMutex_);
        if (file_.isOpen()) {
            file_.flush();
            file_.close();
        }
    }
    if (!complete_ && QFileInfo::exists(partPath_)) {
        QFile::remove(partPath_);
    }
}

QString StreamCacheSession::finalPath() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return finalPath_;
}

QString StreamCacheSession::ext() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return ext_;
}

int64_t StreamCacheSession::totalSize() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return totalSize_;
}

bool StreamCacheSession::acceptRanges() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return acceptRanges_;
}

bool StreamCacheSession::isComplete() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return complete_;
}

bool StreamCacheSession::isFailed() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return failed_;
}

bool StreamCacheSession::isCancelled() const {
    return cancelled_.load(std::memory_order_acquire);
}

QString StreamCacheSession::errorString() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return errorString_;
}

double StreamCacheSession::progressRatio() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (complete_) return 1.0;
    if (totalSize_ <= 0) return 0.0;
    int64_t got = 0;
    for (const auto& r : downloadedRanges_) {
        got += (r.second - r.first);
    }
    return std::clamp(static_cast<double>(got) / static_cast<double>(totalSize_), 0.0, 1.0);
}

void StreamCacheSession::addReader() {
    readerCount_.fetch_add(1, std::memory_order_acq_rel);
}

void StreamCacheSession::releaseReader() {
    const int remaining = readerCount_.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (remaining <= 0 && !downloadPinned_.load(std::memory_order_acquire) && !isComplete()) {
        cancel();
    }
}

void StreamCacheSession::setDownloadPinned(bool pinned) {
    downloadPinned_.store(pinned, std::memory_order_release);
}

void StreamCacheSession::cancel() {
    if (cancelled_.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    if (manager_) {
        manager_->cancelSessionNetwork(cacheKey_);
        manager_->onSessionClosedOrFailed(cacheKey_);
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        stateCv_.notify_all();
    }
}

void StreamCacheSession::mergeRangeLocked(int64_t start, int64_t end) {
    if (end <= start) return;
    QVector<std::pair<int64_t, int64_t>> next;
    next.reserve(downloadedRanges_.size() + 1);
    bool inserted = false;
    for (const auto& r : downloadedRanges_) {
        if (r.second < start) {
            next.push_back(r);
        } else if (end < r.first) {
            if (!inserted) {
                next.push_back({start, end});
                inserted = true;
            }
            next.push_back(r);
        } else {
            start = std::min(start, r.first);
            end = std::max(end, r.second);
        }
    }
    if (!inserted) {
        next.push_back({start, end});
    }
    downloadedRanges_ = std::move(next);
}

int64_t StreamCacheSession::contiguousEndFromLocked(int64_t offset) const {
    for (const auto& r : downloadedRanges_) {
        if (offset >= r.first && offset < r.second) {
            return r.second;
        }
        if (r.first > offset) break;
    }
    return -1;
}

std::pair<int64_t, int64_t> StreamCacheSession::firstMissingGapLocked() const {
    if (totalSize_ <= 0) return {-1, -1};
    int64_t cursor = 0;
    for (const auto& r : downloadedRanges_) {
        if (r.first > cursor) {
            return {cursor, std::min(r.first, totalSize_)};
        }
        cursor = std::max(cursor, r.second);
        if (cursor >= totalSize_) return {-1, -1};
    }
    if (cursor < totalSize_) {
        return {cursor, totalSize_};
    }
    return {-1, -1};
}

std::pair<int64_t, int64_t> StreamCacheSession::firstMissingGap() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return firstMissingGapLocked();
}

void StreamCacheSession::onHeadersReceived(int statusCode,
                                           int64_t totalSize,
                                           bool acceptRanges,
                                           const QString& detectedExt,
                                           int64_t activeStartOffset,
                                           quint64 generation) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    if (generation != netGeneration_ || cancelled_.load(std::memory_order_acquire)) return;

    if (statusCode >= 400) {
        failed_ = true;
        errorString_ = QStringLiteral("HTTP %1 while streaming audio").arg(statusCode);
        stateCv_.notify_all();
        return;
    }

    if (totalSize > 0 && totalSize_ <= 0) {
        totalSize_ = totalSize;
    }
    if (acceptRanges) {
        acceptRanges_ = true;
    }
    if (!detectedExt.isEmpty() && !complete_) {
        const QString norm = normalizeExt(detectedExt);
        if (!norm.isEmpty() && norm != ext_) {
            ext_ = norm;
            const QFileInfo fi(finalPath_);
            finalPath_ = fi.dir().filePath(fi.completeBaseName() + QLatin1Char('.') + ext_);
        }
    }
    currentWriteOffset_ = activeStartOffset;
    activeNetBaseOffset_ = activeStartOffset;
    headersReady_ = true;
    stateCv_.notify_all();
}

void StreamCacheSession::onChunkReceived(int64_t writeOffset, const QByteArray& chunk, quint64 generation) {
    if (chunk.isEmpty() || cancelled_.load(std::memory_order_acquire)) return;
    {
        std::lock_guard<std::mutex> flock(fileMutex_);
        if (!file_.isOpen()) return;
        if (!file_.seek(writeOffset)) return;
        if (file_.write(chunk) != chunk.size()) return;
        file_.flush();
    }

    std::lock_guard<std::mutex> slock(stateMutex_);
    if (generation != netGeneration_) return;
    mergeRangeLocked(writeOffset, writeOffset + chunk.size());
    currentWriteOffset_ = writeOffset + chunk.size();
    requestedJumpOffset_ = -1;
    stateCv_.notify_all();
}

bool StreamCacheSession::finalizeIfCompleteLocked(std::unique_lock<std::mutex>& stateLock) {
    if (complete_) return true;
    if (totalSize_ <= 0 || firstMissingGapLocked().first >= 0) {
        return false;
    }

    const QString part = partPath_;
    const QString finalP = finalPath_;
    const QString key = cacheKey_;
    const QString ext = ext_;
    const int64_t sz = totalSize_;
    const bool hasReaders = readerCount_.load(std::memory_order_acquire) > 0;

    // Close and rename under fileMutex_ before notifying waiters
    {
        std::lock_guard<std::mutex> flock(fileMutex_);
        if (file_.isOpen()) {
            file_.flush();
            file_.close();
        }
        if (QFileInfo::exists(finalP)) {
            QFile::remove(finalP);
        }
        if (QFile::rename(part, finalP)) {
            activePath_ = finalP;
        } else {
            activePath_ = part;
        }
        if (hasReaders) {
            file_.setFileName(activePath_);
            file_.open(QIODevice::ReadOnly);
        }
    }

    complete_ = true;
    finalPath_ = activePath_;
    stateCv_.notify_all();
    stateLock.unlock();

    if (manager_) {
        manager_->onSessionCompletedInternal(key, activePath_, ext, sz);
    }
    return true;
}

void StreamCacheSession::onTransferFinished(bool ok, int httpStatus, const QString& errMsg, quint64 generation) {
    std::unique_lock<std::mutex> lock(stateMutex_);
    if (generation != netGeneration_ || cancelled_.load(std::memory_order_acquire)) return;

    if (!ok) {
        failed_ = true;
        errorString_ = errMsg.isEmpty()
            ? QStringLiteral("Stream transfer failed (HTTP %1)").arg(httpStatus)
            : errMsg;
        stateCv_.notify_all();
        const QString key = cacheKey_;
        const QString err = errorString_;
        lock.unlock();
        if (manager_) {
            manager_->onSessionClosedOrFailed(key);
            emit manager_->sessionFailed(key, err);
        }
        return;
    }

    // If Content-Length was unknown, infer totalSize from downloaded range on clean EOF
    if (totalSize_ <= 0 && !downloadedRanges_.isEmpty()) {
        totalSize_ = downloadedRanges_.last().second;
    }
    headersReady_ = true;

    const auto gap = firstMissingGapLocked();
    if (gap.first >= 0 && acceptRanges_ && manager_) {
        const quint64 nextGen = ++netGeneration_;
        activeNetBaseOffset_ = gap.first;
        currentWriteOffset_ = gap.first;
        auto self = shared_from_this();
        lock.unlock();
        manager_->requestSessionRange(self, gap.first, gap.second - 1, nextGen);
        return;
    }

    if (gap.first < 0 && totalSize_ >= 0) {
        finalizeIfCompleteLocked(lock);
        return;
    }

    // Non-ranged response ended without missing gaps
    if (!acceptRanges_ && !downloadedRanges_.isEmpty()) {
        totalSize_ = downloadedRanges_.last().second;
        finalizeIfCompleteLocked(lock);
        return;
    }

    failed_ = true;
    errorString_ = QStringLiteral("Incomplete stream response");
    stateCv_.notify_all();
    const QString key = cacheKey_;
    lock.unlock();
    if (manager_) {
        manager_->onSessionClosedOrFailed(key);
    }
}

bool StreamCacheSession::waitForHeaders(int timeoutMs, const std::atomic<bool>* abortFlag, bool* timedOutOut) {
    if (timedOutOut) *timedOutOut = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(200, timeoutMs));

    std::unique_lock<std::mutex> lock(stateMutex_);
    while (!headersReady_ && !complete_ && !failed_ && !cancelled_.load(std::memory_order_acquire)) {
        if (abortFlag && abortFlag->load(std::memory_order_acquire)) {
            return false;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            if (timedOutOut) *timedOutOut = true;
            return false;
        }
        stateCv_.wait_for(lock, std::chrono::milliseconds(25));
    }
    return headersReady_ || complete_;
}

int StreamCacheSession::readAt(int64_t offset,
                               uint8_t* dst,
                               int maxLen,
                               int timeoutMs,
                               const std::atomic<bool>* abortFlag,
                               bool* timedOutOut,
                               bool* abortedOut) {
    if (timedOutOut) *timedOutOut = false;
    if (abortedOut) *abortedOut = false;
    if (offset < 0 || !dst || maxLen <= 0) return -1;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(200, timeoutMs));

    while (true) {
        if ((abortFlag && abortFlag->load(std::memory_order_acquire))
            || cancelled_.load(std::memory_order_acquire)) {
            if (abortedOut) *abortedOut = true;
            return -1;
        }

        int64_t availEnd = -1;
        int64_t totalSz = -1;
        bool isDone = false;
        bool isFail = false;
        bool shouldJump = false;
        quint64 jumpGen = 0;

        {
            std::unique_lock<std::mutex> lock(stateMutex_);
            totalSz = totalSize_;
            isDone = complete_;
            isFail = failed_;
            if (totalSz >= 0 && offset >= totalSz) {
                return 0; // EOF
            }
            availEnd = contiguousEndFromLocked(offset);
            if (availEnd <= offset) {
                if (isDone) {
                    return 0;
                }
                if (isFail) {
                    return -1;
                }
                const bool aheadFar = (offset < activeNetBaseOffset_) || (offset > currentWriteOffset_ + 256 * 1024);
                if (acceptRanges_ && aheadFar && requestedJumpOffset_ != offset && manager_) {
                    requestedJumpOffset_ = offset;
                    activeNetBaseOffset_ = offset;
                    currentWriteOffset_ = offset;
                    jumpGen = ++netGeneration_;
                    shouldJump = true;
                }
            }
        }

        if (shouldJump && manager_) {
            manager_->requestSessionRange(shared_from_this(), offset, -1, jumpGen);
        }

        if (availEnd > offset) {
            const int toRead = static_cast<int>(std::min<int64_t>(maxLen, availEnd - offset));
            std::lock_guard<std::mutex> flock(fileMutex_);
            if (!file_.isOpen()) {
                file_.setFileName(activePath_);
                if (!file_.open(isDone ? QIODevice::ReadOnly : QIODevice::ReadWrite)) {
                    return -1;
                }
            }
            if (!file_.seek(offset)) {
                return -1;
            }
            const qint64 n = file_.read(reinterpret_cast<char*>(dst), toRead);
            if (n > 0) {
                return static_cast<int>(n);
            }
            if (isDone) {
                return 0;
            }
        }

        if (std::chrono::steady_clock::now() >= deadline) {
            if (timedOutOut) *timedOutOut = true;
            return -1;
        }

        std::unique_lock<std::mutex> lock(stateMutex_);
        if (contiguousEndFromLocked(offset) <= offset && !complete_ && !failed_
            && !cancelled_.load(std::memory_order_acquire)) {
            stateCv_.wait_for(lock, std::chrono::milliseconds(25));
        }
    }
}

// ============================================================================
// StreamCacheManager
// ============================================================================

StreamCacheManager::StreamCacheManager(const QString& cacheDir,
                                       qint64 maxCacheBytes,
                                       QObject* parent)
    : QObject(parent)
    , cacheDir_(QDir::cleanPath(cacheDir))
    , maxCacheBytes_(std::max<qint64>(1024 * 1024, maxCacheBytes)) {
    QDir().mkpath(cacheDir_);
    cleanStalePartFiles();
    loadIndex();
    enforceLruLimit();
}

void StreamCacheManager::ensureNetworkThreadStarted() {
    std::lock_guard<std::mutex> lock(workerInitMutex_);
    if (worker_ && netThread_.isRunning()) {
        return;
    }
    if (!worker_) {
        worker_ = new StreamNetworkWorker();
        worker_->moveToThread(&netThread_);
        connect(&netThread_, &QThread::started, worker_, &StreamNetworkWorker::init);
        connect(&netThread_, &QThread::finished, worker_, &QObject::deleteLater);
    }
    if (!netThread_.isRunning()) {
        netThread_.start();
    }
}

StreamCacheManager::~StreamCacheManager() {
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        for (auto it = activeSessions_.begin(); it != activeSessions_.end(); ++it) {
            if (auto s = it.value().lock()) {
                s->cancelled_.store(true, std::memory_order_release);
            }
        }
        activeSessions_.clear();
    }
    if (netThread_.isRunning()) {
        netThread_.quit();
        netThread_.wait();
    } else if (worker_) {
        delete worker_;
        worker_ = nullptr;
    }
}

QString StreamCacheManager::makeCacheKey(const QString& source, const QString& remoteId, const QString& quality) {
    const QString raw = source + QLatin1Char(':') + remoteId + QLatin1Char(':') + quality;
    return QString::fromLatin1(QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha1).toHex().left(24));
}

QString StreamCacheManager::inferExtension(const QString& contentType, const QUrl& url, const QString& hint) {
    const QString ct = contentType.section(QLatin1Char(';'), 0, 0).trimmed().toLower();
    if (ct == QStringLiteral("audio/flac") || ct == QStringLiteral("audio/x-flac")) return QStringLiteral("flac");
    if (ct == QStringLiteral("audio/mpeg") || ct == QStringLiteral("audio/mp3")) return QStringLiteral("mp3");
    if (ct == QStringLiteral("audio/mp4") || ct == QStringLiteral("audio/aac")
        || ct == QStringLiteral("audio/x-m4a")) return QStringLiteral("m4a");
    if (ct == QStringLiteral("audio/ogg") || ct == QStringLiteral("application/ogg")) return QStringLiteral("ogg");
    if (ct == QStringLiteral("audio/wav") || ct == QStringLiteral("audio/x-wav")) return QStringLiteral("wav");
    if (ct == QStringLiteral("audio/webm")) return QStringLiteral("webm");

    const QString pathExt = normalizeExt(QFileInfo(url.path()).suffix());
    if (pathExt == QStringLiteral("flac") || pathExt == QStringLiteral("mp3")
        || pathExt == QStringLiteral("m4a") || pathExt == QStringLiteral("aac")
        || pathExt == QStringLiteral("ogg") || pathExt == QStringLiteral("wav")
        || pathExt == QStringLiteral("opus")) {
        return pathExt;
    }

    const QString normHint = normalizeExt(hint);
    return normHint.isEmpty() ? QStringLiteral("mp3") : normHint;
}

qint64 StreamCacheManager::maxCacheBytes() const {
    std::lock_guard<std::mutex> lock(indexMutex_);
    return maxCacheBytes_;
}

void StreamCacheManager::setMaxCacheBytes(qint64 maxBytes) {
    {
        std::lock_guard<std::mutex> lock(indexMutex_);
        const qint64 clamped = std::max<qint64>(1024 * 1024, maxBytes);
        if (maxCacheBytes_ == clamped) return;
        maxCacheBytes_ = clamped;
    }
    enforceLruLimit();
    emit cacheStatsChanged();
}

qint64 StreamCacheManager::currentCacheBytes() const {
    std::lock_guard<std::mutex> lock(indexMutex_);
    qint64 sum = 0;
    for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it) {
        sum += std::max<qint64>(0, it.value().sizeBytes);
    }
    return sum;
}

QString StreamCacheManager::findCompletedCacheFile(const QString& cacheKey) {
    std::lock_guard<std::mutex> lock(indexMutex_);
    auto it = entries_.find(cacheKey);
    if (it == entries_.end()) return {};

    const QString path = QDir(cacheDir_).filePath(it->fileName);
    if (!QFileInfo::exists(path)) {
        entries_.erase(it);
        saveIndexLocked();
        return {};
    }
    it->lastAccessMs = QDateTime::currentMSecsSinceEpoch();
    saveIndexLocked();
    return path;
}

std::shared_ptr<StreamCacheSession> StreamCacheManager::activeSessionForKey(const QString& cacheKey) const {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    auto it = activeSessions_.constFind(cacheKey);
    if (it != activeSessions_.constEnd()) {
        if (auto existing = it.value().lock()) {
            if (!existing->isCancelled() && !existing->isFailed()) {
                return existing;
            }
        }
    }
    return {};
}

std::shared_ptr<StreamCacheSession> StreamCacheManager::openOrStartSession(const QString& cacheKey,
                                                                           const QUrl& url,
                                                                           const QString& cookie,
                                                                           const QString& extHint) {
    if (auto existing = activeSessionForKey(cacheKey)) {
        return existing;
    }

    const QString ext = inferExtension({}, url, extHint);
    const QString partPath = QDir(cacheDir_).filePath(cacheKey + QStringLiteral(".part"));
    const QString finalPath = QDir(cacheDir_).filePath(cacheKey + QLatin1Char('.') + ext);

    auto session = std::make_shared<StreamCacheSession>(this, cacheKey, partPath, finalPath, url, cookie, ext);
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        activeSessions_.insert(cacheKey, session);
    }

    if (!session->isFailed()) {
        requestSessionRange(session, 0, -1, 0);
    }
    return session;
}

void StreamCacheManager::requestSessionRange(const std::shared_ptr<StreamCacheSession>& session,
                                             int64_t startOffset,
                                             int64_t endOffset,
                                             quint64 generation) {
    if (!session || session->isCancelled()) return;
    ensureNetworkThreadStarted();
    if (!worker_) return;
    QMetaObject::invokeMethod(
        worker_,
        "startRangeTransfer",
        Qt::QueuedConnection,
        Q_ARG(QString, session->cacheKey_),
        Q_ARG(std::shared_ptr<virela::StreamCacheSession>, session),
        Q_ARG(QUrl, session->url_),
        Q_ARG(QString, session->cookie_),
        Q_ARG(qint64, static_cast<qint64>(startOffset)),
        Q_ARG(qint64, static_cast<qint64>(endOffset)),
        Q_ARG(quint64, generation),
        Q_ARG(int, 0));
}

void StreamCacheManager::cancelSessionNetwork(const QString& cacheKey) {
    if (!worker_) return;
    QMetaObject::invokeMethod(worker_, "cancelTransfer", Qt::QueuedConnection, Q_ARG(QString, cacheKey));
}

void StreamCacheManager::onSessionCompletedInternal(const QString& cacheKey,
                                                    const QString& finalPath,
                                                    const QString& ext,
                                                    qint64 sizeBytes) {
    const QFileInfo fi(finalPath);
    const qint64 actualSize = fi.exists() ? fi.size() : sizeBytes;
    {
        std::lock_guard<std::mutex> lock(indexMutex_);
        const auto prevIt = entries_.constFind(cacheKey);
        if (prevIt != entries_.constEnd() && prevIt->fileName != fi.fileName() && !prevIt->fileName.isEmpty()) {
            QFile::remove(QDir(cacheDir_).filePath(prevIt->fileName));
        }
        CacheEntry e;
        e.key = cacheKey;
        e.fileName = fi.fileName();
        e.ext = ext;
        e.sizeBytes = actualSize;
        e.lastAccessMs = QDateTime::currentMSecsSinceEpoch();
        entries_.insert(cacheKey, e);
        saveIndexLocked();
    }
    {
        std::lock_guard<std::mutex> lock(sessionMutex_);
        activeSessions_.remove(cacheKey);
    }
    enforceLruLimit();
    QMetaObject::invokeMethod(this, [this, cacheKey, finalPath]() {
        emit cacheStatsChanged();
        emit sessionCompleted(cacheKey, finalPath);
    }, Qt::QueuedConnection);
}

void StreamCacheManager::onSessionClosedOrFailed(const QString& cacheKey) {
    std::lock_guard<std::mutex> lock(sessionMutex_);
    activeSessions_.remove(cacheKey);
}

void StreamCacheManager::enforceLruLimit() {
    QVector<QString> protectedKeys;
    {
        std::lock_guard<std::mutex> slock(sessionMutex_);
        for (auto it = activeSessions_.constBegin(); it != activeSessions_.constEnd(); ++it) {
            if (auto s = it.value().lock()) {
                if (s->readerCount() > 0 || s->isDownloadPinned()) {
                    protectedKeys.push_back(it.key());
                }
            }
        }
    }

    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(indexMutex_);
        qint64 total = 0;
        QVector<CacheEntry> list;
        list.reserve(entries_.size());
        for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it) {
            total += std::max<qint64>(0, it.value().sizeBytes);
            list.push_back(it.value());
        }
        if (total <= maxCacheBytes_) return;

        std::sort(list.begin(), list.end(), [](const CacheEntry& a, const CacheEntry& b) {
            return a.lastAccessMs < b.lastAccessMs;
        });

        for (const CacheEntry& e : list) {
            if (total <= maxCacheBytes_) break;
            if (protectedKeys.contains(e.key)) continue;
            QFile::remove(QDir(cacheDir_).filePath(e.fileName));
            total -= std::max<qint64>(0, e.sizeBytes);
            entries_.remove(e.key);
            changed = true;
        }
        if (changed) {
            saveIndexLocked();
        }
    }
    if (changed) {
        QMetaObject::invokeMethod(this, &StreamCacheManager::cacheStatsChanged, Qt::QueuedConnection);
    }
}

void StreamCacheManager::clearCache() {
    QVector<QString> protectedFiles;
    {
        std::lock_guard<std::mutex> slock(sessionMutex_);
        for (auto it = activeSessions_.constBegin(); it != activeSessions_.constEnd(); ++it) {
            if (auto s = it.value().lock()) {
                if (s->readerCount() > 0 || s->isDownloadPinned()) {
                    protectedFiles.push_back(it.key());
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lock(indexMutex_);
        QMutableHashIterator<QString, CacheEntry> it(entries_);
        while (it.hasNext()) {
            it.next();
            if (protectedFiles.contains(it.key())) continue;
            QFile::remove(QDir(cacheDir_).filePath(it.value().fileName));
            it.remove();
        }
        saveIndexLocked();
    }
    emit cacheStatsChanged();
}

void StreamCacheManager::cleanStalePartFiles() {
    QDir dir(cacheDir_);
    const QFileInfoList parts = dir.entryInfoList({QStringLiteral("*.part")}, QDir::Files);
    for (const QFileInfo& fi : parts) {
        QFile::remove(fi.absoluteFilePath());
    }
}

void StreamCacheManager::loadIndex() {
    std::lock_guard<std::mutex> lock(indexMutex_);
    entries_.clear();
    const QString idxPath = QDir(cacheDir_).filePath(QStringLiteral("index.json"));
    QFile f(idxPath);
    if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());

    auto loadArray = [this](const QJsonArray& arr, bool purgeLegacyMp3) {
        for (const QJsonValue& val : arr) {
            if (!val.isObject()) continue;
            const QJsonObject obj = val.toObject();
            CacheEntry e;
            e.key = obj.value(QStringLiteral("key")).toString();
            e.fileName = obj.value(QStringLiteral("fileName")).toString();
            e.ext = obj.value(QStringLiteral("ext")).toString();
            e.sizeBytes = static_cast<qint64>(obj.value(QStringLiteral("sizeBytes")).toDouble());
            e.lastAccessMs = static_cast<qint64>(obj.value(QStringLiteral("lastAccessMs")).toDouble());
            if (e.key.isEmpty() || e.fileName.isEmpty()) continue;

            const QString fp = QDir(cacheDir_).filePath(e.fileName);
            if (purgeLegacyMp3
                && (e.ext.compare(QStringLiteral("mp3"), Qt::CaseInsensitive) == 0
                    || e.fileName.endsWith(QStringLiteral(".mp3"), Qt::CaseInsensitive))) {
                QFile::remove(fp);
                continue;
            }
            if (QFileInfo::exists(fp)) {
                entries_.insert(e.key, e);
            }
        }
    };

    if (doc.isArray()) {
        // Migrate legacy v1 index (pre-ENABLE_FLAC): remove stale MP3 entries once so
        // lossless/Hi-Res playback automatically upgrades to FLAC on next play.
        loadArray(doc.array(), true);
        saveIndexLocked();
    } else if (doc.isObject()) {
        loadArray(doc.object().value(QStringLiteral("entries")).toArray(), false);
    }
}

void StreamCacheManager::saveIndexLocked() const {
    QDir().mkpath(cacheDir_);
    QJsonArray arr;
    for (auto it = entries_.constBegin(); it != entries_.constEnd(); ++it) {
        const CacheEntry& e = it.value();
        QJsonObject obj;
        obj.insert(QStringLiteral("key"), e.key);
        obj.insert(QStringLiteral("fileName"), e.fileName);
        obj.insert(QStringLiteral("ext"), e.ext);
        obj.insert(QStringLiteral("sizeBytes"), static_cast<double>(e.sizeBytes));
        obj.insert(QStringLiteral("lastAccessMs"), static_cast<double>(e.lastAccessMs));
        arr.append(obj);
    }
    QJsonObject root;
    root.insert(QStringLiteral("version"), 2);
    root.insert(QStringLiteral("entries"), arr);
    QSaveFile f(QDir(cacheDir_).filePath(QStringLiteral("index.json")));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        f.commit();
    }
}

BlockingHttpResult StreamCacheManager::runBlockingHttpGet(const QUrl& url,
                                                            const QString& cookie,
                                                            int timeoutMs,
                                                            const std::atomic<bool>* abortFlag,
                                                            bool followRedirects) {
    ensureNetworkThreadStarted();
    if (!worker_ || !netThread_.isRunning()) {
        BlockingHttpResult err;
        err.errorString = QStringLiteral("Network worker unavailable");
        return err;
    }

    auto waitState = std::make_shared<BlockingHttpWait>();
    QMetaObject::invokeMethod(
        worker_,
        "performBlockingGet",
        Qt::QueuedConnection,
        Q_ARG(QUrl, url),
        Q_ARG(QString, cookie),
        Q_ARG(int, timeoutMs),
        Q_ARG(bool, followRedirects),
        Q_ARG(int, 0),
        Q_ARG(std::shared_ptr<virela::BlockingHttpWait>, waitState));

    std::unique_lock<std::mutex> lock(waitState->mutex);
    while (!waitState->done) {
        if (abortFlag && abortFlag->load(std::memory_order_acquire)) {
            waitState->abortRequested.store(true, std::memory_order_release);
        }
        waitState->cv.wait_for(lock, std::chrono::milliseconds(25));
    }
    return waitState->result;
}

} // namespace virela

#include "StreamCacheManager.moc"
