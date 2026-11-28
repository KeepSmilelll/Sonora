#include "audio/FFmpegDecoder.h"
#include "audio/IAudioOutput.h"
#include "audio/PlaybackEngine.h"
#include "core/AppSettings.h"
#include "core/PlaybackController.h"
#include "core/ThemeManager.h"
#include "library/CoverArtProvider.h"
#include "library/LibraryDatabase.h"
#include "library/LibraryScanner.h"
#include "lyrics/LyricsController.h"
#include "lyrics/LyricsParser.h"
#include "models/LibraryModels.h"
#include "online/AudioMetadataWriter.h"
#include "online/LocalBackendManager.h"
#include "online/OnlineMusicController.h"
#include "online/OnlineMusicService.h"
#include "online/StreamCacheManager.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtTest/QtTest>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

using namespace virela;

namespace {

class MockAudioOutput final : public IAudioOutput {
public:
    QVector<AudioDeviceInfo> enumerateDevices() const override {
        return {{QStringLiteral("mock-0"), QStringLiteral("Mock DAC"), true}};
    }

    bool open(const OutputConfig& config,
              int requestedSampleRate,
              int requestedChannels,
              AudioRenderCallback callback,
              ActiveOutputStatus* statusOut) override {
        cb_ = std::move(callback);
        status_.sampleRate = requestedSampleRate > 0 ? requestedSampleRate : 48000;
        status_.channels = requestedChannels > 0 ? requestedChannels : 2;
        status_.hardwareFormat = QStringLiteral("32-bit Float");
        status_.exclusiveActive = (config.mode == OutputMode::WasapiExclusive);
        status_.bitPerfectActive = status_.exclusiveActive && config.bitPerfectBypass;
        status_.deviceName = QStringLiteral("Mock DAC");
        open_ = true;
        if (statusOut) *statusOut = status_;
        return true;
    }

    bool start() override { started_ = open_; return started_; }
    void stop() override { started_ = false; }
    void close() override { started_ = false; open_ = false; cb_ = nullptr; }
    bool isOpen() const override { return open_; }
    bool isStarted() const override { return started_; }
    ActiveOutputStatus activeStatus() const override { return status_; }

    void pull(float* out, uint32_t frames) {
        if (cb_) cb_(out, frames, static_cast<uint32_t>(status_.channels));
    }

private:
    AudioRenderCallback cb_;
    ActiveOutputStatus status_;
    bool open_ = false;
    bool started_ = false;
};

struct HttpReq {
    QString method;
    QString rawTarget;
    QString path;
    QUrlQuery query;
    QHash<QString, QString> headers;
};

struct HttpResp {
    int status = 200;
    QByteArray reason = "OK";
    QByteArray contentType = "application/json; charset=utf-8";
    QByteArray body;
    QList<std::pair<QByteArray, QByteArray>> extraHeaders;
    bool stallIndefinitely = false;
};

class LoopbackHttpServer final {
public:
    using HandlerFn = std::function<HttpResp(const HttpReq&)>;

    explicit LoopbackHttpServer(HandlerFn handler)
        : handler_(std::move(handler)) {}

    ~LoopbackHttpServer() {
        stop();
    }

    LoopbackHttpServer(const LoopbackHttpServer&) = delete;
    LoopbackHttpServer& operator=(const LoopbackHttpServer&) = delete;

    bool listen() {
        if (thread_.isRunning()) {
            return port_ != 0;
        }
        worker_ = new Worker(this);
        worker_->moveToThread(&thread_);
        thread_.start();

        bool ok = false;
        quint16 boundPort = 0;
        QMetaObject::invokeMethod(worker_, [this, &ok, &boundPort]() {
            worker_->server = std::make_unique<QTcpServer>();
            QObject::connect(worker_->server.get(), &QTcpServer::newConnection, worker_, [this]() {
                while (worker_->server) {
                    QTcpSocket* sock = worker_->server->nextPendingConnection();
                    if (!sock) break;
                    handleSocket(sock);
                }
            });
            ok = worker_->server->listen(QHostAddress::LocalHost, 0);
            if (ok) {
                boundPort = worker_->server->serverPort();
            }
        }, Qt::BlockingQueuedConnection);
        port_ = boundPort;
        return ok;
    }

    void stop() {
        if (thread_.isRunning() && worker_) {
            QMetaObject::invokeMethod(worker_, [this]() {
                for (const QPointer<QTcpSocket>& sock : std::as_const(worker_->openSockets)) {
                    if (sock) {
                        sock->disconnect();
                        sock->abort();
                    }
                }
                worker_->openSockets.clear();
                if (worker_->server) {
                    worker_->server->close();
                    worker_->server.reset();
                }
            }, Qt::BlockingQueuedConnection);
            thread_.quit();
            thread_.wait();
        }
        delete worker_;
        worker_ = nullptr;
        port_ = 0;
    }

    QString baseUrl() const {
        return QStringLiteral("http://127.0.0.1:%1").arg(port_);
    }

    int requestCount(const QString& pathPrefix) const {
        QMutexLocker locker(&reqMutex_);
        int total = 0;
        for (const auto& r : requests_) {
            if (r.path.startsWith(pathPrefix)) ++total;
        }
        return total;
    }

    QVector<HttpReq> requests() const {
        QMutexLocker locker(&reqMutex_);
        return requests_;
    }

private:
    class Worker final : public QObject {
    public:
        explicit Worker(LoopbackHttpServer* owner) : owner_(owner) {}
        LoopbackHttpServer* owner_ = nullptr;
        std::unique_ptr<QTcpServer> server;
        QVector<QPointer<QTcpSocket>> openSockets;
    };

    void handleSocket(QTcpSocket* sock) {
        worker_->openSockets.append(sock);
        auto buf = std::make_shared<QByteArray>();
        QObject::connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
        QObject::connect(sock, &QTcpSocket::readyRead, worker_, [this, sock, buf]() {
            buf->append(sock->readAll());
            const int hdrEnd = buf->indexOf("\r\n\r\n");
            if (hdrEnd < 0) return;

            const QList<QByteArray> lines = buf->left(hdrEnd).split('\n');
            if (lines.isEmpty()) {
                sock->disconnectFromHost();
                return;
            }
            const QList<QByteArray> reqLine = lines.first().trimmed().split(' ');
            HttpReq req;
            if (reqLine.size() >= 2) {
                req.method = QString::fromLatin1(reqLine[0]);
                req.rawTarget = QString::fromUtf8(reqLine[1]);
                const QUrl u(QStringLiteral("http://localhost") + req.rawTarget);
                req.path = u.path();
                req.query = QUrlQuery(u);
            }
            for (int i = 1; i < lines.size(); ++i) {
                const QByteArray line = lines[i].trimmed();
                const int colon = line.indexOf(':');
                if (colon > 0) {
                    req.headers.insert(
                        QString::fromLatin1(line.left(colon)).trimmed().toLower(),
                        QString::fromUtf8(line.mid(colon + 1)).trimmed());
                }
            }
            {
                QMutexLocker locker(&reqMutex_);
                requests_.push_back(req);
            }
            buf->clear();

            const HttpResp resp = handler_ ? handler_(req) : HttpResp{};
            if (resp.stallIndefinitely) {
                // Keep socket open without sending headers/body to simulate network hang
                return;
            }

            QByteArray raw;
            raw += "HTTP/1.1 " + QByteArray::number(resp.status) + " " + resp.reason + "\r\n";
            if (!resp.contentType.isEmpty()) {
                raw += "Content-Type: " + resp.contentType + "\r\n";
            }
            for (const auto& h : resp.extraHeaders) {
                raw += h.first + ": " + h.second + "\r\n";
            }
            raw += "Content-Length: " + QByteArray::number(resp.body.size()) + "\r\n";
            raw += "Connection: close\r\n\r\n";
            raw += resp.body;
            sock->write(raw);
            sock->flush();
            sock->disconnectFromHost();
        });
    }

    QThread thread_;
    Worker* worker_ = nullptr;
    quint16 port_ = 0;
    HandlerFn handler_;
    mutable QMutex reqMutex_;
    QVector<HttpReq> requests_;
};

bool writeSyntheticWav(const QString& wavPath, int freqHz, int sampleRate, double durationSec) {
    const uint32_t channels = 2;
    const uint32_t totalFrames = static_cast<uint32_t>(std::llround(sampleRate * durationSec));
    const uint32_t dataBytes = totalFrames * channels * sizeof(int16_t);
    const uint32_t riffSize = 36 + dataBytes;

    QFile f(wavPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    auto writeU32 = [&](uint32_t v) {
        char b[4] = {char(v & 0xFF), char((v >> 8) & 0xFF), char((v >> 16) & 0xFF), char((v >> 24) & 0xFF)};
        f.write(b, 4);
    };
    auto writeU16 = [&](uint16_t v) {
        char b[2] = {char(v & 0xFF), char((v >> 8) & 0xFF)};
        f.write(b, 2);
    };

    f.write("RIFF", 4);
    writeU32(riffSize);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    writeU32(16);
    writeU16(1);
    writeU16(channels);
    writeU32(static_cast<uint32_t>(sampleRate));
    writeU32(static_cast<uint32_t>(sampleRate) * channels * 2);
    writeU16(channels * 2);
    writeU16(16);
    f.write("data", 4);
    writeU32(dataBytes);

    std::vector<int16_t> samples(totalFrames * channels);
    constexpr double kTwoPi = 6.28318530717958647692;
    for (uint32_t i = 0; i < totalFrames; ++i) {
        const double s = 0.45 * std::sin(kTwoPi * freqHz * static_cast<double>(i) / static_cast<double>(sampleRate));
        const int16_t pcm = static_cast<int16_t>(std::lround(s * 32767.0));
        samples[i * 2 + 0] = pcm;
        samples[i * 2 + 1] = pcm;
    }
    return f.write(reinterpret_cast<const char*>(samples.data()), dataBytes) == static_cast<qint64>(dataBytes);
}

QString resolveFfmpegExecutable() {
#ifdef VIRELA_FFMPEG_EXECUTABLE
    const QString compiled = QStringLiteral(VIRELA_FFMPEG_EXECUTABLE);
    if (QFileInfo::exists(compiled)) return compiled;
#endif
    const QString envRoot = qEnvironmentVariable("FFMPEG_ROOT");
    if (!envRoot.isEmpty()) {
#ifdef _WIN32
        const QString candidate = QDir(envRoot).filePath(QStringLiteral("bin/ffmpeg.exe"));
#else
        const QString candidate = QDir(envRoot).filePath(QStringLiteral("bin/ffmpeg"));
#endif
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return QStringLiteral("ffmpeg");
}

bool generateSyntheticAudio(const QString& outPath,
                            int freqHz,
                            int sampleRate,
                            double durationSec,
                            const QStringList& extraArgs) {
    const QString tmpWav = outPath + QStringLiteral(".src.wav");
    if (!writeSyntheticWav(tmpWav, freqHz, sampleRate, durationSec)) return false;

    QProcess proc;
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    const QString ffmpegExe = resolveFfmpegExecutable();
#ifdef _WIN32
    QString binDir;
#ifdef VIRELA_FFMPEG_BIN_DIR
    binDir = QStringLiteral(VIRELA_FFMPEG_BIN_DIR);
#endif
    if (binDir.isEmpty()) binDir = QFileInfo(ffmpegExe).absolutePath();
    if (!binDir.isEmpty()) {
        env.insert(QStringLiteral("PATH"),
                   QDir::toNativeSeparators(binDir) + QLatin1Char(';') + env.value(QStringLiteral("PATH")));
    }
#endif
    proc.setProcessEnvironment(env);

    QStringList args = {
        QStringLiteral("-y"), QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-i"), tmpWav
    };
    args.append(extraArgs);
    args.append(outPath);

    proc.start(ffmpegExe, args);
    const bool ok = proc.waitForFinished(15000) && proc.exitStatus() == QProcess::NormalExit && proc.exitCode() == 0;
    QFile::remove(tmpWav);
    return ok;
}

QByteArray makeSolidJpeg(const QColor& col, int size = 128) {
    QImage img(size, size, QImage::Format_RGB32);
    img.fill(col);
    QByteArray out;
    QBuffer buf(&out);
    buf.open(QIODevice::WriteOnly);
    img.save(&buf, "JPG", 88);
    return out;
}

HttpResp serveRangeBytes(const HttpReq& req, const QByteArray& payload, const QByteArray& mime) {
    HttpResp r;
    r.contentType = mime;
    r.extraHeaders.append(std::make_pair(QByteArray("Accept-Ranges"), QByteArray("bytes")));
    const QString rangeHdr = req.headers.value(QStringLiteral("range"));
    if (!rangeHdr.isEmpty() && rangeHdr.startsWith(QStringLiteral("bytes="))) {
        const QString spec = rangeHdr.mid(6);
        const QStringList parts = spec.split(QLatin1Char('-'));
        const qint64 total = payload.size();
        qint64 start = parts[0].isEmpty() ? 0 : parts[0].toLongLong();
        qint64 end = (parts.size() > 1 && !parts[1].isEmpty()) ? parts[1].toLongLong() : (total - 1);
        start = std::clamp<qint64>(start, 0, total - 1);
        end = std::clamp<qint64>(end, start, total - 1);
        r.status = 206;
        r.reason = "Partial Content";
        r.extraHeaders.append(std::make_pair(
            QByteArray("Content-Range"),
            QStringLiteral("bytes %1-%2/%3").arg(start).arg(end).arg(total).toLatin1()));
        r.body = payload.mid(static_cast<qsizetype>(start), static_cast<qsizetype>(end - start + 1));
        return r;
    }
    r.status = 200;
    r.body = payload;
    return r;
}

} // namespace

class TestOnlineMusic : public QObject {
    Q_OBJECT

private slots:
    void settingsDefaultsAndPersistence() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString cfgPath = dir.filePath(QStringLiteral("settings.json"));

        {
            AppSettings s(cfgPath);
            // Zero hardcoded public third-party URLs
            QVERIFY(s.onlineNeteaseApiUrl().isEmpty());
            QVERIFY(s.onlineMetingApiUrl().isEmpty());
            QCOMPARE(s.onlineDirectUrlPriority(), QStringLiteral("netease"));
            QVERIFY(s.onlineAutoFallback());
            QVERIFY(s.onlineUnblockTrialFallback());
            QCOMPARE(s.onlineAudioQuality(), QStringLiteral("lossless"));
            QVERIFY(s.onlineCookie().isEmpty());
            QCOMPARE(s.onlineCacheMaxMb(), 1024);
            QVERIFY(!s.onlineDownloadDir().isEmpty());
            QVERIFY(!s.onlineAutoStartLocalServices());
            QVERIFY(s.onlineNeteaseServiceDir().isEmpty());
            QVERIFY(s.onlineMetingServiceDir().isEmpty());

            s.setOnlineNeteaseApiUrl(QStringLiteral("http://127.0.0.1:3000///"));
            s.setOnlineMetingApiUrl(QStringLiteral("http://127.0.0.1:3001/api"));
            s.setOnlineDirectUrlPriority(QStringLiteral("meting"));
            s.setOnlineAutoFallback(false);
            s.setOnlineUnblockTrialFallback(false);
            s.setOnlineAudioQuality(QStringLiteral("hires"));
            s.setOnlineCookie(QStringLiteral("MUSIC_U=abc123"));
            s.setOnlineCacheMaxMb(2048);
            s.setOnlineDownloadDir(dir.filePath(QStringLiteral("dl")));
            s.setOnlineAutoStartLocalServices(true);
            s.setOnlineNeteaseServiceDir(dir.filePath(QStringLiteral("ncm")));
            s.setOnlineMetingServiceDir(dir.filePath(QStringLiteral("met")));
        }

        AppSettings reloaded(cfgPath);
        QCOMPARE(reloaded.onlineNeteaseApiUrl(), QStringLiteral("http://127.0.0.1:3000"));
        QCOMPARE(reloaded.onlineMetingApiUrl(), QStringLiteral("http://127.0.0.1:3001/api"));
        QCOMPARE(reloaded.onlineDirectUrlPriority(), QStringLiteral("meting"));
        QVERIFY(!reloaded.onlineAutoFallback());
        QVERIFY(!reloaded.onlineUnblockTrialFallback());
        QCOMPARE(reloaded.onlineAudioQuality(), QStringLiteral("hires"));
        QCOMPARE(reloaded.onlineCookie(), QStringLiteral("MUSIC_U=abc123"));
        QCOMPARE(reloaded.onlineCacheMaxMb(), 2048);
        QCOMPARE(reloaded.onlineDownloadDir(), QDir::cleanPath(dir.filePath(QStringLiteral("dl"))));
        QVERIFY(reloaded.onlineAutoStartLocalServices());
        QCOMPARE(reloaded.onlineNeteaseServiceDir(), QDir::cleanPath(dir.filePath(QStringLiteral("ncm"))));
        QCOMPARE(reloaded.onlineMetingServiceDir(), QDir::cleanPath(dir.filePath(QStringLiteral("met"))));
    }

    void searchCanLeavePlaylistDetailAndClearResults() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        LoopbackHttpServer server([](const HttpReq& req) {
            HttpResp resp;
            if (req.path == QStringLiteral("/playlist/detail")) {
                resp.body = R"({"code":200,"playlist":{"id":31,"name":"Test Playlist","tracks":[]}})";
            } else if (req.path == QStringLiteral("/cloudsearch")) {
                resp.body = R"({"code":200,"result":{"songs":[{"id":101,"name":"Test Song","dt":269000}]}})";
            }
            return resp;
        });
        QVERIFY(server.listen());
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setOnlineNeteaseApiUrl(server.baseUrl());
        OnlineMusicService service(&settings, nullptr);
        OnlineMusicController controller(&service, nullptr, &settings, nullptr, nullptr, nullptr, nullptr);

        controller.search(QStringLiteral("Test"), QStringLiteral("songs"));
        QTRY_COMPARE(controller.songsModel()->property("count").toInt(), 1);

        controller.search(QStringLiteral("https://music.163.com/#/playlist?id=31"), QStringLiteral("songs"));
        QVERIFY(controller.inDetail());
        QCOMPARE(controller.searchQuery(), QStringLiteral("Test"));

        controller.search(QStringLiteral("Next"), QStringLiteral("songs"));
        QVERIFY(!controller.inDetail());
        QCOMPARE(controller.searchQuery(), QStringLiteral("Next"));
        QCOMPARE(controller.songsModel()->property("count").toInt(), 0);
        QTRY_COMPARE(controller.songsModel()->property("count").toInt(), 1);

        controller.search(QString(), QStringLiteral("songs"));
        QVERIFY(controller.searchQuery().isEmpty());
        QVERIFY(!controller.isBusy());
        QCOMPARE(controller.songsModel()->property("count").toInt(), 0);
    }

    void playlistInputExtraction() {
        QCOMPARE(OnlineMusicService::extractPlaylistId(QStringLiteral("3778678")), QStringLiteral("3778678"));
        QCOMPARE(OnlineMusicService::extractPlaylistId(QStringLiteral("https://music.163.com/#/playlist?id=24381616")),
                 QStringLiteral("24381616"));
        QCOMPARE(OnlineMusicService::extractPlaylistId(QStringLiteral("https://music.163.com/playlist?id=99887766&userid=1")),
                 QStringLiteral("99887766"));
        QCOMPARE(OnlineMusicService::extractPlaylistId(QStringLiteral("https://y.music.163.com/m/playlist?id=55667788")),
                 QStringLiteral("55667788"));
        QCOMPARE(OnlineMusicService::extractPlaylistId(QStringLiteral("https://music.163.com/playlist/11223344/")),
                 QStringLiteral("11223344"));
        QCOMPARE(OnlineMusicService::extractPlaylistId(QStringLiteral("分享歌单 https://music.163.com/#/playlist?id=77665544 来自网易云")),
                 QStringLiteral("77665544"));
        QVERIFY(OnlineMusicService::extractPlaylistId(QStringLiteral("not a playlist")).isEmpty());

        QCOMPARE(
            OnlineMusicService::sizedCoverUrl(QStringLiteral("http://p4.music.126.net/abc/123.jpg"), 120),
            QStringLiteral("http://p4.music.126.net/abc/123.jpg?param=120y120"));
        QCOMPARE(
            OnlineMusicService::sizedCoverUrl(QStringLiteral("http://p4.music.126.net/abc/123.jpg?param=50y50"), 120),
            QStringLiteral("http://p4.music.126.net/abc/123.jpg?param=50y50"));
        QCOMPARE(
            OnlineMusicService::sizedCoverUrl(QStringLiteral("image://cover/abcd?size=96"), 120),
            QStringLiteral("image://cover/abcd?size=96"));
        QCOMPARE(
            OnlineMusicService::sizedCoverUrl(QStringLiteral("https://example.com/cover.jpg"), 120),
            QStringLiteral("https://example.com/cover.jpg"));
    }

    void neteaseAndMetingApisAndFallback() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        std::atomic<bool> neteaseShouldFailUrl{false};
        std::atomic<bool> neteaseShouldMatchUrl{false};
        QMutex cookieMutex;
        QString observedCookie;

        LoopbackHttpServer neteaseSrv([&](const HttpReq& req) -> HttpResp {
            {
                QMutexLocker locker(&cookieMutex);
                observedCookie = req.headers.value(QStringLiteral("cookie"));
            }
            HttpResp resp;
            if (req.path == QStringLiteral("/search/hot")) {
                resp.body = R"({"code":200,"result":{"hots":[]}})";
            } else if (req.path == QStringLiteral("/cloudsearch")) {
                const QString type = req.query.queryItemValue(QStringLiteral("type"));
                if (type == QStringLiteral("1")) {
                    resp.body = R"({"code":200,"result":{"songs":[{"id":101,"name":"晴天","dt":269000,"ar":[{"id":11,"name":"周杰伦"}],"al":{"id":21,"name":"叶惠美","picUrl":"http://127.0.0.1/c.jpg"},"hr":{"br":999000}}]}})";
                } else if (type == QStringLiteral("10")) {
                    resp.body = R"({"code":200,"result":{"albums":[{"id":21,"name":"叶惠美","size":11,"publishTime":1059609600000,"artist":{"id":11,"name":"周杰伦"}}]}})";
                } else if (type == QStringLiteral("100")) {
                    resp.body = R"({"code":200,"result":{"artists":[{"id":11,"name":"周杰伦","albumSize":15,"musicSize":200}]}})";
                } else if (type == QStringLiteral("1000")) {
                    resp.body = R"({"code":200,"result":{"playlists":[{"id":31,"name":"深夜无损","trackCount":2,"playCount":999,"creator":{"nickname":"Tester"}}]}})";
                }
            } else if (req.path == QStringLiteral("/playlist/detail")) {
                resp.body = R"({"code":200,"playlist":{"id":31,"name":"深夜无损","trackCount":1,"creator":{"nickname":"Tester"},"tracks":[{"id":101,"name":"晴天","dt":269000,"ar":[{"id":11,"name":"周杰伦"}],"al":{"id":21,"name":"叶惠美"}}]}})";
            } else if (req.path == QStringLiteral("/album")) {
                resp.body = R"({"code":200,"album":{"id":21,"name":"叶惠美","artist":{"name":"周杰伦"}},"songs":[{"id":101,"name":"晴天","dt":269000,"ar":[{"id":11,"name":"周杰伦"}]}]})";
            } else if (req.path == QStringLiteral("/artists")) {
                resp.body = R"({"code":200,"artist":{"id":11,"name":"周杰伦"},"hotSongs":[{"id":101,"name":"晴天","dt":269000,"ar":[{"id":11,"name":"周杰伦"}],"al":{"id":21,"name":"叶惠美"}}]})";
            } else if (req.path == QStringLiteral("/song/url/v1")) {
                if (neteaseShouldFailUrl.load(std::memory_order_acquire)) {
                    resp.body = R"({"code":200,"data":[{"id":101,"url":null,"freeTrialInfo":{"start":0,"end":30}}]})";
                } else {
                    resp.body = R"({"code":200,"data":[{"id":101,"url":"http://127.0.0.1:9/netease.flac","type":"flac","level":"lossless","br":999000,"freeTrialInfo":null}]})";
                }
            } else if (req.path == QStringLiteral("/song/url/match")) {
                if (neteaseShouldMatchUrl.load(std::memory_order_acquire)) {
                    resp.body = R"({"code":200,"data":"http://127.0.0.1:9/unblocked.mp3","proxyUrl":""})";
                } else {
                    resp.status = 404;
                    resp.body = R"({"code":404})";
                }
            } else {
                resp.status = 404;
            }
            return resp;
        });
        QVERIFY(neteaseSrv.listen());

        LoopbackHttpServer metingSrv([&](const HttpReq& req) -> HttpResp {
            HttpResp resp;
            const QString type = req.query.queryItemValue(QStringLiteral("type"));
            if (type == QStringLiteral("song")) {
                resp.body = R"([{"id":"101","name":"晴天"}])";
            } else if (type == QStringLiteral("url")) {
                resp.status = 302;
                resp.reason = "Found";
                resp.contentType = "";
                resp.extraHeaders.append(std::make_pair(QByteArray("Location"), QByteArray("http://127.0.0.1:9/meting.mp3")));
            } else {
                resp.status = 404;
            }
            return resp;
        });
        QVERIFY(metingSrv.listen());

        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setOnlineNeteaseApiUrl(neteaseSrv.baseUrl());
        settings.setOnlineMetingApiUrl(metingSrv.baseUrl());
        settings.setOnlineCookie(QStringLiteral("MUSIC_U=testcookie"));
        settings.setOnlineDirectUrlPriority(QStringLiteral("netease"));
        settings.setOnlineAutoFallback(true);

        StreamCacheManager cache(dir.filePath(QStringLiteral("cache")));
        OnlineMusicService svc(&settings, &cache);

        // Connection test
        bool connDone = false;
        BackendTestResult netRes, metRes;
        svc.testConnections([&](BackendTestResult n, BackendTestResult m) {
            netRes = n;
            metRes = m;
            connDone = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(connDone, 3000);
        QVERIFY(netRes.ok);
        QVERIFY(netRes.latencyMs >= 0);
        QVERIFY(metRes.ok);
        {
            QMutexLocker locker(&cookieMutex);
            QCOMPARE(observedCookie, QStringLiteral("MUSIC_U=testcookie"));
        }

        // Aggregate searches
        QVector<TrackInfo> songs;
        bool sDone = false;
        svc.searchSongs(QStringLiteral("晴天"), 20, 0, [&](bool ok, QVector<TrackInfo> t, QString) {
            QVERIFY(ok);
            songs = std::move(t);
            sDone = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(sDone, 3000);
        QCOMPARE(songs.size(), 1);
        QCOMPARE(songs.first().remoteId, QStringLiteral("101"));
        QCOMPARE(songs.first().formatBadge, QStringLiteral("Hi-Res"));

        QVector<OnlineAlbumItem> albums;
        bool alDone = false;
        svc.searchAlbums(QStringLiteral("叶惠美"), 20, 0, [&](bool ok, QVector<OnlineAlbumItem> a, QString) {
            QVERIFY(ok);
            albums = std::move(a);
            alDone = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(alDone, 3000);
        QCOMPARE(albums.size(), 1);
        QCOMPARE(albums.first().name, QStringLiteral("叶惠美"));

        QVector<OnlineArtistItem> artists;
        bool arDone = false;
        svc.searchArtists(QStringLiteral("周杰伦"), 20, 0, [&](bool ok, QVector<OnlineArtistItem> a, QString) {
            QVERIFY(ok);
            artists = std::move(a);
            arDone = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(arDone, 3000);
        QCOMPARE(artists.size(), 1);

        QVector<OnlinePlaylistItem> playlists;
        bool plDone = false;
        svc.searchPlaylists(QStringLiteral("深夜"), 20, 0, [&](bool ok, QVector<OnlinePlaylistItem> p, QString) {
            QVERIFY(ok);
            playlists = std::move(p);
            plDone = true;
        });
        QTRY_VERIFY_WITH_TIMEOUT(plDone, 3000);
        QCOMPARE(playlists.size(), 1);

        // Priority: Netease first
        const ResolvedStreamInfo r1 = svc.resolveStreamBlocking(songs.first());
        QVERIFY(r1.ok);
        QCOMPARE(r1.backendUsed, QStringLiteral("netease"));
        QCOMPARE(r1.streamUrl, QStringLiteral("http://127.0.0.1:9/netease.flac"));

        // Priority: Meting first (302 redirect capture)
        settings.setOnlineDirectUrlPriority(QStringLiteral("meting"));
        const ResolvedStreamInfo r2 = svc.resolveStreamBlocking(songs.first());
        QVERIFY(r2.ok);
        QCOMPARE(r2.backendUsed, QStringLiteral("meting"));
        QCOMPARE(r2.streamUrl, QStringLiteral("http://127.0.0.1:9/meting.mp3"));

        // Auto fallback from Netease null/trial -> Meting (when /song/url/match is unavailable)
        settings.setOnlineDirectUrlPriority(QStringLiteral("netease"));
        neteaseShouldFailUrl.store(true, std::memory_order_release);
        neteaseShouldMatchUrl.store(false, std::memory_order_release);
        const ResolvedStreamInfo r3 = svc.resolveStreamBlocking(songs.first());
        QVERIFY(r3.ok);
        QCOMPARE(r3.backendUsed, QStringLiteral("meting"));

        // Auto rescue via self-hosted /song/url/match when Netease returns VIP trial / null
        neteaseShouldMatchUrl.store(true, std::memory_order_release);
        const ResolvedStreamInfo r4 = svc.resolveStreamBlocking(songs.first());
        QVERIFY(r4.ok);
        QCOMPARE(r4.backendUsed, QStringLiteral("netease-match"));
        QVERIFY(!r4.isTrial);
        QCOMPARE(r4.codecHint, QStringLiteral("mp3"));
        QCOMPARE(r4.streamUrl, QStringLiteral("http://127.0.0.1:9/unblocked.mp3"));

        // When onlineUnblockTrialFallback is disabled, /song/url/match is skipped -> falls back to Meting
        settings.setOnlineUnblockTrialFallback(false);
        const ResolvedStreamInfo r5 = svc.resolveStreamBlocking(songs.first());
        QVERIFY(r5.ok);
        QCOMPARE(r5.backendUsed, QStringLiteral("meting"));
    }

    void onlineLyricsYrcWithTlyricAndSidecarRoundTrip() {
        const QByteArray lyricJson =
            "{\"code\":200,"
            "\"yrc\":{\"lyric\":\"{\\\"t\\\":0,\\\"c\\\":[]}\\n[1000,2000](1000,500,0)Hel(1500,500,0)lo\\n[4000,1800](4000,900,0)Wo(4900,900,0)rld\\n\"},"
            "\"lrc\":{\"lyric\":\"[00:01.00]Hello\\n[00:04.00]World\\n\"},"
            "\"tlyric\":{\"lyric\":\"[00:01.08]你好\\n[00:04.00]世界\\n\"}}";

        const OnlineLyricsResult parsed = OnlineMusicService::parseNeteaseLyricsJson(lyricJson);
        QVERIFY(parsed.ok);
        QVERIFY(!parsed.yrc.isEmpty());
        QVERIFY(!parsed.tlyric.isEmpty());

        LyricsController ctrl;
        ctrl.loadRawLyrics(parsed.yrc, parsed.tlyric, parsed.sourceLabel);
        QVERIFY(ctrl.hasLyrics());
        QVERIFY(ctrl.isSynced());
        QVERIFY(ctrl.hasEnhancedWords());
        QCOMPARE(ctrl.rowCount(), 2);
        QCOMPARE(ctrl.data(ctrl.index(0), LyricsController::TextRole).toString(), QStringLiteral("Hello"));
        QCOMPARE(ctrl.data(ctrl.index(0), LyricsController::TranslationRole).toString(), QStringLiteral("你好"));
        QCOMPARE(ctrl.data(ctrl.index(1), LyricsController::TranslationRole).toString(), QStringLiteral("世界"));

        // Verify sidecar .yrc write + reload preserves both word tokens and translations
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString ext;
        const QString sidecarText = LyricsParser::composeSidecarLyrics(parsed.yrc, parsed.lrc, parsed.tlyric, &ext);
        QCOMPARE(ext, QStringLiteral("yrc"));

        const QString songPath = dir.filePath(QStringLiteral("test_song.flac"));
        QFile sidecarFile(dir.filePath(QStringLiteral("test_song.yrc")));
        QVERIFY(sidecarFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        sidecarFile.write(sidecarText.toUtf8());
        sidecarFile.close();

        const ParsedLyrics reloaded = LyricsParser::loadForTrack(songPath);
        QCOMPARE(reloaded.sourceLabel, QStringLiteral("YRC"));
        QCOMPARE(reloaded.lines.size(), 2);
        QCOMPARE(reloaded.lines[0].words.size(), 2);
        QCOMPARE(reloaded.lines[0].translation, QStringLiteral("你好"));
        QCOMPARE(reloaded.lines[1].translation, QStringLiteral("世界"));
    }

    void streamCacheProgressiveSeekLruAndZeroTraffic() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString srcFlac = dir.filePath(QStringLiteral("source.flac"));
        QVERIFY(generateSyntheticAudio(srcFlac, 660, 44100, 3.0, {QStringLiteral("-c:a"), QStringLiteral("flac")}));
        QFile sf(srcFlac);
        QVERIFY(sf.open(QIODevice::ReadOnly));
        const QByteArray flacBytes = sf.readAll();
        sf.close();
        QVERIFY(flacBytes.size() > 32768);

        LoopbackHttpServer cdnSrv([&](const HttpReq& req) -> HttpResp {
            return serveRangeBytes(req, flacBytes, "audio/flac");
        });
        QVERIFY(cdnSrv.listen());

        const QString cacheDir = dir.filePath(QStringLiteral("stream_cache"));
        StreamCacheManager cache(cacheDir, 1024 * 1024);

        const QString key1 = StreamCacheManager::makeCacheKey(QStringLiteral("netease"), QStringLiteral("5001"), QStringLiteral("lossless"));
        auto session = cache.openOrStartSession(key1, QUrl(cdnSrv.baseUrl() + QStringLiteral("/track1.flac")));
        QVERIFY(session);

        // Zero-traffic check: opening same active key reuses same session instance
        auto reused = cache.openOrStartSession(key1, QUrl(cdnSrv.baseUrl() + QStringLiteral("/track1.flac")));
        QCOMPARE(reused.get(), session.get());

        FFmpegDecoder dec;
        QString err;
        QVERIFY2(dec.openStream(session, 44100, 2, 176400, &err), qPrintable(err));
        QVERIFY(dec.trackInfo().durationMs >= 2800);

        // Seek forward and backward smoothly over progressive stream cache
        QVERIFY(dec.seekRelativeMs(1800));
        std::vector<float> buf(2048 * 2);
        bool eof = false;
        const size_t nSeek = dec.decodeFrames(buf.data(), 1024, eof);
        QVERIFY(nSeek > 0);
        QVERIFY(dec.seekRelativeMs(200));
        QVERIFY(dec.decodeFrames(buf.data(), 1024, eof) > 0);
        dec.close();

        // Wait for session completion and verify completed cache hit
        QTRY_VERIFY_WITH_TIMEOUT(!cache.findCompletedCacheFile(key1).isEmpty(), 5000);
        QCOMPARE(QFileInfo(cache.findCompletedCacheFile(key1)).size(), flacBytes.size());

        // Verify LRU eviction enforces cap when a second large entry exceeds maxCacheBytes
        const qint64 tightCap = std::max<qint64>(1024 * 1024, flacBytes.size() + 4096);
        cache.setMaxCacheBytes(tightCap);
        const QString key2 = StreamCacheManager::makeCacheKey(QStringLiteral("netease"), QStringLiteral("5002"), QStringLiteral("lossless"));
        auto s2 = cache.openOrStartSession(key2, QUrl(cdnSrv.baseUrl() + QStringLiteral("/track2.flac")));
        s2->setDownloadPinned(true);
        QTRY_VERIFY_WITH_TIMEOUT(!cache.findCompletedCacheFile(key2).isEmpty(), 5000);

        // key1 + key2 wouldn't fit if cap < 2 * flacBytes.size()
        if (flacBytes.size() * 2 > tightCap) {
            QVERIFY(cache.currentCacheBytes() <= tightCap);
        }
    }

    void ffmpegDecoderInterruptAndTimeoutProtection() {
        LoopbackHttpServer stalledSrv([](const HttpReq&) -> HttpResp {
            HttpResp r;
            r.stallIndefinitely = true;
            return r;
        });
        QVERIFY(stalledSrv.listen());

        // 1. Timeout protection test
        {
            FFmpegDecoder dec;
            dec.setNetworkTimeoutMs(600);
            QElapsedTimer t;
            t.start();
            QString err;
            const bool ok = dec.open(stalledSrv.baseUrl() + QStringLiteral("/hang.mp3"), 0, 2, 176400, 0, 0, &err);
            QVERIFY(!ok);
            QVERIFY(dec.wasTimedOut());
            QVERIFY2(t.elapsed() < 3500, qPrintable(QStringLiteral("Timeout took too long: %1 ms").arg(t.elapsed())));
        }

        // 2. Immediate cross-thread abort protection test (simulating track skip while stalled)
        {
            FFmpegDecoder dec;
            dec.setNetworkTimeoutMs(10000);
            std::atomic<bool> abortFlag{false};
            dec.setExternalAbortFlag(&abortFlag);
            QTimer::singleShot(150, this, [&]() {
                abortFlag.store(true, std::memory_order_release);
            });
            QElapsedTimer t;
            t.start();
            // Pump Qt timer in background thread? QTimer fires on main thread, so run open in worker thread!
            bool ok = true;
            std::thread worker([&]() {
                QString err;
                ok = dec.open(stalledSrv.baseUrl() + QStringLiteral("/hang2.mp3"), 0, 2, 176400, 0, 0, &err);
            });
            QTRY_VERIFY_WITH_TIMEOUT(abortFlag.load(), 1000);
            worker.join();
            QVERIFY(!ok);
            QVERIFY(dec.wasAborted());
            QVERIFY2(t.elapsed() < 2500, qPrintable(QStringLiteral("Abort took too long: %1 ms").arg(t.elapsed())));
        }
    }

    void zeroTrafficDownloadMetadataCoverLyricsAndLibraryIngest() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString flacSrc = dir.filePath(QStringLiteral("raw.flac"));
        const QString mp3Src = dir.filePath(QStringLiteral("raw.mp3"));
        QVERIFY(generateSyntheticAudio(flacSrc, 520, 44100, 0.8, {QStringLiteral("-c:a"), QStringLiteral("flac")}));
        QVERIFY(generateSyntheticAudio(mp3Src, 520, 44100, 0.8, {QStringLiteral("-c:a"), QStringLiteral("libmp3lame"), QStringLiteral("-b:a"), QStringLiteral("192k")}));

        QFile ff(flacSrc);
        QVERIFY(ff.open(QIODevice::ReadOnly));
        const QByteArray flacBytes = ff.readAll();
        ff.close();

        QFile mf(mp3Src);
        QVERIFY(mf.open(QIODevice::ReadOnly));
        const QByteArray mp3Bytes = mf.readAll();
        mf.close();

        const QByteArray coverJpg = makeSolidJpeg(QColor(240, 72, 110), 160);

        QString srvBaseUrl;
        LoopbackHttpServer srv([&](const HttpReq& req) -> HttpResp {
            HttpResp r;
            if (req.path == QStringLiteral("/audio/cached.flac")) {
                return serveRangeBytes(req, flacBytes, "audio/flac");
            }
            if (req.path == QStringLiteral("/audio/direct.mp3")) {
                return serveRangeBytes(req, mp3Bytes, "audio/mpeg");
            }
            if (req.path == QStringLiteral("/cover.jpg")) {
                r.contentType = "image/jpeg";
                r.body = coverJpg;
                return r;
            }
            if (req.path == QStringLiteral("/lyric/new")) {
                r.body = R"({"code":200,"yrc":{"lyric":"[0,1500](0,700,0)Vire(700,800,0)la\n"},"tlyric":{"lyric":"[00:00.00]维蕾拉\n"}})";
                return r;
            }
            if (req.path == QStringLiteral("/song/url/v1")) {
                const QString id = req.query.queryItemValue(QStringLiteral("id"));
                if (id == QStringLiteral("701")) {
                    r.body = QStringLiteral(R"({"code":200,"data":[{"id":701,"url":"%1/audio/cached.flac","type":"flac","level":"lossless","br":999000}]})")
                                 .arg(srvBaseUrl).toUtf8();
                } else {
                    r.body = QStringLiteral(R"({"code":200,"data":[{"id":702,"url":"%1/audio/direct.mp3","type":"mp3","level":"exhigh","br":320000}]})")
                                 .arg(srvBaseUrl).toUtf8();
                }
                return r;
            }
            r.status = 404;
            return r;
        });
        QVERIFY(srv.listen());
        srvBaseUrl = srv.baseUrl();

        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setOnlineNeteaseApiUrl(srv.baseUrl());
        settings.setOnlineAudioQuality(QStringLiteral("lossless"));
        const QString dlDir = dir.filePath(QStringLiteral("downloads"));
        settings.setOnlineDownloadDir(dlDir);

        LibraryDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        CoverArtProvider covers(dir.filePath(QStringLiteral("covers")));
        LibraryScanner scanner(&db, &covers);

        StreamCacheManager cache(dir.filePath(QStringLiteral("stream_cache")));
        OnlineMusicService service(&settings, &cache);
        PlaybackEngine engine(std::make_unique<MockAudioOutput>());
        ThemeManager theme(&settings);
        PlaybackController player(&engine, &settings, &theme);
        LibraryController libCtrl(&db, &scanner, &covers, &settings, &player);
        LyricsController lyrCtrl;
        OnlineMusicController onlineCtrl(&service, &cache, &settings, &player, &libCtrl, &lyrCtrl, &covers);

        // Pre-populate cache with track 701 (simulates already-played track)
        const QString key701 = StreamCacheManager::makeCacheKey(QStringLiteral("netease"), QStringLiteral("701"), QStringLiteral("lossless"));
        auto preSession = cache.openOrStartSession(key701, QUrl(srv.baseUrl() + QStringLiteral("/audio/cached.flac")));
        preSession->setDownloadPinned(true);
        QTRY_VERIFY_WITH_TIMEOUT(!cache.findCompletedCacheFile(key701).isEmpty(), 5000);
        const int audioHitsBeforeDownload = srv.requestCount(QStringLiteral("/audio/cached.flac"));
        QCOMPARE(audioHitsBeforeDownload, 1);

        // Inject track 701 (FLAC cached) and 702 (MP3 uncached) via mock playlist detail
        TrackInfo tFlac;
        tFlac.source = QStringLiteral("netease");
        tFlac.remoteId = QStringLiteral("701");
        tFlac.title = QStringLiteral("Cached Symphony");
        tFlac.artist = QStringLiteral("Virela Artist");
        tFlac.album = QStringLiteral("Virela Album");
        tFlac.coverUrl = srv.baseUrl() + QStringLiteral("/cover.jpg");

        TrackInfo tMp3;
        tMp3.source = QStringLiteral("netease");
        tMp3.remoteId = QStringLiteral("702");
        tMp3.title = QStringLiteral("Fresh Track");
        tMp3.artist = QStringLiteral("Virela Artist");
        tMp3.album = QStringLiteral("Virela Album");
        tMp3.coverUrl = srv.baseUrl() + QStringLiteral("/cover.jpg");

        // Test AudioMetadataWriter directly + zero-traffic controller flow
        const QString dlFlac = QDir(dlDir).filePath(QStringLiteral("Virela Artist - Cached Symphony.flac"));
        const QString dlMp3 = QDir(dlDir).filePath(QStringLiteral("Virela Artist - Fresh Track.mp3"));

        // Use OnlineMusicController::downloadCurrentTrack for 701 -> MUST make ZERO extra /audio/cached.flac requests!
        engine.setOnlineStreamResolver([&](const TrackInfo& tr, const std::atomic<bool>*) -> ResolvedOnlinePlayback {
            ResolvedOnlinePlayback out;
            const QString k = StreamCacheManager::makeCacheKey(tr.source, tr.remoteId, settings.onlineAudioQuality());
            out.localCachePath = cache.findCompletedCacheFile(k);
            return out;
        });

        QObject::connect(&engine, &PlaybackEngine::currentTrackChanged,
                         &onlineCtrl, [&](const TrackInfo& tr, int) {
            onlineCtrl.onCurrentTrackChanged(tr);
        });

        QSignalSpy trackChangedSpy(&engine, &PlaybackEngine::currentTrackChanged);
        engine.playTrack(tFlac);
        QTRY_COMPARE_WITH_TIMEOUT(trackChangedSpy.count(), 1, 3000);
        QCOMPARE(player.bufferedRatio(), 1.0);
        onlineCtrl.downloadCurrentTrack();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(dlFlac), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(onlineCtrl.activeDownloadCount() == 0, 5000);
        QCOMPARE(onlineCtrl.currentTrackDownloadStatus(), QStringLiteral("done"));
        QCOMPARE(onlineCtrl.downloadTasks().size(), 1);
        QCOMPARE(onlineCtrl.downloadTasks().first().toMap().value(QStringLiteral("status")).toString(),
                 QStringLiteral("done"));
        onlineCtrl.clearFinishedDownloads();
        QCOMPARE(onlineCtrl.downloadTasks().size(), 0);

        // Zero-traffic verification!
        QCOMPARE(srv.requestCount(QStringLiteral("/audio/cached.flac")), audioHitsBeforeDownload);

        // Verify embedded metadata, embedded cover, sidecar .yrc, and LibraryDatabase ingestion
        TrackInfo probedFlac;
        QVERIFY(FFmpegDecoder::probeFile(dlFlac, probedFlac));
        QCOMPARE(probedFlac.title, QStringLiteral("Cached Symphony"));
        QCOMPARE(probedFlac.artist, QStringLiteral("Virela Artist"));
        QCOMPARE(probedFlac.album, QStringLiteral("Virela Album"));
        QVERIFY(!FFmpegDecoder::extractEmbeddedCover(dlFlac).isEmpty());
        QVERIFY(QFileInfo::exists(QDir(dlDir).filePath(QStringLiteral("Virela Artist - Cached Symphony.yrc"))));
        QCOMPARE(db.totalTrackCount(), 1);

        // Also verify AudioMetadataWriter on MP3 + ingestLocalFile
        QString mp3Err;
        QVERIFY2(AudioMetadataWriter::writeMetadataAndCover(
            mp3Src, dlMp3, tMp3, coverJpg, QStringLiteral("[00:00.00]Line"), &mp3Err), qPrintable(mp3Err));
        QVERIFY(libCtrl.ingestLocalFile(dlMp3));
        TrackInfo probedMp3;
        QVERIFY(FFmpegDecoder::probeFile(dlMp3, probedMp3));
        QCOMPARE(probedMp3.title, QStringLiteral("Fresh Track"));
        QCOMPARE(probedMp3.artist, QStringLiteral("Virela Artist"));
        QVERIFY(!FFmpegDecoder::extractEmbeddedCover(dlMp3).isEmpty());
        QCOMPARE(db.totalTrackCount(), 2);
    }

    void playbackEngineOnlineQueueStoresRemoteIdOnly() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        const QString flacSrc = dir.filePath(QStringLiteral("online_q.flac"));
        QVERIFY(generateSyntheticAudio(flacSrc, 440, 44100, 0.5, {QStringLiteral("-c:a"), QStringLiteral("flac")}));
        QFile f(flacSrc);
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QByteArray flacBytes = f.readAll();
        f.close();

        LoopbackHttpServer srv([&](const HttpReq& req) -> HttpResp {
            if (req.path == QStringLiteral("/stream.flac")) {
                return serveRangeBytes(req, flacBytes, "audio/flac");
            }
            HttpResp r;
            r.status = 404;
            return r;
        });
        QVERIFY(srv.listen());

        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        StreamCacheManager cache(dir.filePath(QStringLiteral("cache")));
        auto mockRaw = std::make_unique<MockAudioOutput>();
        MockAudioOutput* mockPtr = mockRaw.get();
        PlaybackEngine engine(std::move(mockRaw));
        engine.equalizer().setEnabled(true);
        engine.equalizer().setBandGainDb(0, 3.0);

        int resolveCalls = 0;
        engine.setOnlineStreamResolver([&](const TrackInfo& tr, const std::atomic<bool>*) -> ResolvedOnlinePlayback {
            ++resolveCalls;
            ResolvedOnlinePlayback res;
            const QString key = StreamCacheManager::makeCacheKey(tr.source, tr.remoteId, QStringLiteral("lossless"));
            res.session = cache.openOrStartSession(key, QUrl(srv.baseUrl() + QStringLiteral("/stream.flac")));
            return res;
        });

        TrackInfo q1;
        q1.source = QStringLiteral("netease");
        q1.remoteId = QStringLiteral("9001");
        q1.title = QStringLiteral("Online Queue Track");
        q1.artist = QStringLiteral("Online Artist");
        q1.durationMs = 500;

        QSignalSpy trackSpy(&engine, &PlaybackEngine::currentTrackChanged);
        engine.setQueue({q1}, 0, true);
        QTRY_VERIFY_WITH_TIMEOUT(trackSpy.count() >= 1 && mockPtr->isStarted(), 4000);
        QCOMPARE(resolveCalls, 1);

        // Queue entry MUST retain source + remoteId and NEVER persist transient URLs in filePath
        const auto q = engine.queue();
        QCOMPARE(q.size(), 1);
        QCOMPARE(q.first().source, QStringLiteral("netease"));
        QCOMPARE(q.first().remoteId, QStringLiteral("9001"));
        QVERIFY(q.first().filePath.isEmpty());

        // Render PCM through EQ + FFT to verify full DSP/Spectrum reuse
        std::vector<float> pcm(1024 * 2, 0.0f);
        mockPtr->pull(pcm.data(), 1024);
        double energy = 0.0;
        for (float v : pcm) energy += v * v;
        QVERIFY(energy > 0.0);
    }

    void localBackendManagerStartAndStopLifecycle() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        if (LocalBackendManager::findNodeExecutable().isEmpty()) {
            QSKIP("Node.js runtime unavailable in test environment");
        }

        auto reservePort = []() -> quint16 {
            QTcpServer s;
            if (!s.listen(QHostAddress::LocalHost, 0)) return 0;
            const quint16 p = s.serverPort();
            s.close();
            return p;
        };
        const quint16 portA = reservePort();
        const quint16 portB = reservePort();
        QVERIFY(portA != 0 && portB != 0 && portA != portB);

        const QString ncmDir = dir.filePath(QStringLiteral("ncm_srv"));
        const QString metDir = dir.filePath(QStringLiteral("met_srv"));
        QVERIFY(QDir().mkpath(ncmDir));
        QVERIFY(QDir().mkpath(metDir));

        {
            QFile f(QDir(ncmDir).filePath(QStringLiteral("server.js")));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(
                "const http = require('http');\n"
                "const host = process.env.HOST || '127.0.0.1';\n"
                "const port = Number(process.env.PORT || 3000);\n"
                "const s = http.createServer((req, res) => {\n"
                "  res.writeHead(200, {'Content-Type': 'application/json'});\n"
                "  res.end('{\"ok\":true,\"service\":\"ncm\"}');\n"
                "});\n"
                "const bye = () => s.close(() => process.exit(0));\n"
                "process.on('SIGTERM', bye); process.on('SIGINT', bye);\n"
                "if (process.env.VIRELA_MANAGED === '1') { process.stdin.resume(); process.stdin.on('end', bye); process.stdin.on('close', bye); }\n"
                "s.listen(port, host);\n");
        }
        {
            QFile f(QDir(metDir).filePath(QStringLiteral("server.mjs")));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
            f.write(
                "import http from 'node:http';\n"
                "const host = process.env.HOST || '127.0.0.1';\n"
                "const port = Number(process.env.PORT || 3001);\n"
                "const s = http.createServer((req, res) => {\n"
                "  res.writeHead(200, {'Content-Type': 'application/json'});\n"
                "  res.end('{\"ok\":true,\"service\":\"met\"}');\n"
                "});\n"
                "const bye = () => s.close(() => process.exit(0));\n"
                "process.on('SIGTERM', bye); process.on('SIGINT', bye);\n"
                "if (process.env.VIRELA_MANAGED === '1') { process.stdin.resume(); process.stdin.on('end', bye); process.stdin.on('close', bye); }\n"
                "s.listen(port, host);\n");
        }

        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        settings.setOnlineNeteaseApiUrl(QStringLiteral("http://127.0.0.1:%1").arg(portA));
        settings.setOnlineMetingApiUrl(QStringLiteral("http://127.0.0.1:%1/api").arg(portB));
        settings.setOnlineNeteaseServiceDir(ncmDir);
        settings.setOnlineMetingServiceDir(metDir);
        settings.setOnlineAutoStartLocalServices(true);

        LocalBackendManager mgr(&settings);
        mgr.startConfiguredServices();

        QTRY_VERIFY_WITH_TIMEOUT(
            LocalBackendManager::isPortAccepting(QStringLiteral("127.0.0.1"), portA, 80)
            && LocalBackendManager::isPortAccepting(QStringLiteral("127.0.0.1"), portB, 80),
            5000);
        QVERIFY(mgr.neteaseRunning());
        QVERIFY(mgr.metingRunning());

        // Verify live HTTP 200 responses from both managed processes
        auto probeHttp = [](quint16 port) -> QByteArray {
            QTcpSocket sock;
            sock.connectToHost(QStringLiteral("127.0.0.1"), port);
            if (!sock.waitForConnected(1000)) return {};
            sock.write("GET /health HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
            sock.waitForBytesWritten(1000);
            QByteArray data;
            while (sock.waitForReadyRead(1000)) {
                data.append(sock.readAll());
            }
            return data;
        };
        QVERIFY(probeHttp(portA).contains("200 OK"));
        QVERIFY(probeHttp(portB).contains("200 OK"));

        mgr.stopAllServices();
        QVERIFY(!mgr.neteaseRunning());
        QVERIFY(!mgr.metingRunning());
        QTRY_VERIFY_WITH_TIMEOUT(
            !LocalBackendManager::isPortAccepting(QStringLiteral("127.0.0.1"), portA, 80)
            && !LocalBackendManager::isPortAccepting(QStringLiteral("127.0.0.1"), portB, 80),
            4000);
    }

    void detectPlaylistLinkIdAndBufferedRatioReset() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        StreamCacheManager cache(dir.filePath(QStringLiteral("cache")));
        OnlineMusicService service(&settings, &cache);
        PlaybackEngine engine(std::make_unique<MockAudioOutput>());
        ThemeManager theme(&settings);
        PlaybackController player(&engine, &settings, &theme);
        OnlineMusicController ctrl(&service, &cache, &settings, &player, nullptr, nullptr, nullptr);

        QCOMPARE(ctrl.detectPlaylistLinkId(QStringLiteral("https://music.163.com/#/playlist?id=24381616")),
                 QStringLiteral("24381616"));
        QCOMPARE(ctrl.detectPlaylistLinkId(QStringLiteral("分享歌单 https://music.163.com/playlist?id=987654321")),
                 QStringLiteral("987654321"));
        QCOMPARE(ctrl.detectPlaylistLinkId(QStringLiteral("24381616")),
                 QStringLiteral("24381616"));
        // Short numeric queries (like album years) and plain keywords must NOT trigger playlist detection
        QVERIFY(ctrl.detectPlaylistLinkId(QStringLiteral("1989")).isEmpty());
        QVERIFY(ctrl.detectPlaylistLinkId(QStringLiteral("2025")).isEmpty());
        QVERIFY(ctrl.detectPlaylistLinkId(QStringLiteral("Jay Chou")).isEmpty());

        QCOMPARE(player.bufferedRatio(), 0.0);
        engine.stop();
        QCOMPARE(engine.bufferedRatio(), 0.0);
    }

    void discoverParsersAndFallbackEndpoints() {
        // 1. Static parser coverage (primary & secondary payload formats)
        {
            const QByteArray hotPrimary = QByteArrayLiteral(
                "{\"code\":200,\"data\":[{\"searchWord\":\"Jay Chou\",\"content\":\"Popular\",\"score\":99000,\"iconType\":1},{\"searchWord\":\"Chopin\",\"content\":\"Classical\",\"score\":42000,\"iconType\":0}]}");
            QString err;
            const auto hot = OnlineMusicService::parseNeteaseHotSearchJson(hotPrimary, &err);
            QVERIFY(err.isEmpty());
            QCOMPARE(hot.size(), 2);
            QCOMPARE(hot[0].searchWord, QStringLiteral("Jay Chou"));
            QCOMPARE(hot[0].score, 99000);

            const QByteArray hotFallback = QByteArrayLiteral(
                "{\"code\":200,\"result\":{\"hots\":[{\"first\":\"Bach\",\"second\":1,\"iconType\":0}]}}");
            const auto hotFb = OnlineMusicService::parseNeteaseHotSearchJson(hotFallback, &err);
            QVERIFY(err.isEmpty());
            QCOMPARE(hotFb.size(), 1);
            QCOMPARE(hotFb[0].searchWord, QStringLiteral("Bach"));

            const QByteArray recPrimary = QByteArrayLiteral(
                "{\"code\":200,\"result\":[{\"id\":778899,\"name\":\"Late Night Acoustics\",\"picUrl\":\"https://img.example.com/p1.jpg\",\"trackCount\":24,\"playCount\":120000,\"copywriter\":\"Curated Pick\"}]}");
            const auto rec = OnlineMusicService::parseNeteaseRecommendedPlaylistsJson(recPrimary, &err);
            QVERIFY(err.isEmpty());
            QCOMPARE(rec.size(), 1);
            QCOMPARE(rec[0].id, QStringLiteral("778899"));
            QCOMPARE(rec[0].trackCount, 24);

            const QByteArray newPrimary = QByteArrayLiteral(
                "{\"code\":200,\"result\":[{\"id\":556677,\"name\":\"Midnight Waltz\",\"picUrl\":\"https://img.example.com/n1.jpg\",\"song\":{\"id\":556677,\"name\":\"Midnight Waltz\",\"duration\":215000,\"artists\":[{\"id\":11,\"name\":\"Claire\"}],\"album\":{\"id\":22,\"name\":\"Moonlight\",\"picUrl\":\"https://img.example.com/n1.jpg\"},\"hrMusic\":{\"sr\":96000}}}]}");
            const auto news = OnlineMusicService::parseNeteaseNewSongsJson(newPrimary, &err);
            QVERIFY(err.isEmpty());
            QCOMPARE(news.size(), 1);
            QCOMPARE(news[0].remoteId, QStringLiteral("556677"));
            QCOMPARE(news[0].title, QStringLiteral("Midnight Waltz"));
            QCOMPARE(news[0].artist, QStringLiteral("Claire"));
            QCOMPARE(news[0].formatBadge, QStringLiteral("Hi-Res"));
        }

        // 2. Zero network calls when Netease URL is empty, and secondary endpoint fallback via loopback server
        bool useFallbackMode = false;
        LoopbackHttpServer server([&](const HttpReq& req) -> HttpResp {
            HttpResp resp;
            if (!useFallbackMode) {
                if (req.path == QStringLiteral("/search/hot/detail")) {
                    resp.status = 200;
                    resp.body = QByteArrayLiteral("{\"code\":200,\"data\":[{\"searchWord\":\"Debussy\",\"score\":5500}]}");
                    return resp;
                }
                if (req.path == QStringLiteral("/personalized")) {
                    resp.status = 200;
                    resp.body = QByteArrayLiteral("{\"code\":200,\"result\":[{\"id\":123456,\"name\":\"Impressionist Mix\",\"picUrl\":\"http://127.0.0.1/c.jpg\",\"trackCount\":15}]}");
                    return resp;
                }
                if (req.path == QStringLiteral("/personalized/newsong")) {
                    resp.status = 200;
                    resp.body = QByteArrayLiteral("{\"code\":200,\"result\":[{\"id\":9001,\"name\":\"Arabesque\",\"picUrl\":\"http://127.0.0.1/a.jpg\",\"song\":{\"id\":9001,\"name\":\"Arabesque\",\"duration\":180000,\"artists\":[{\"id\":1,\"name\":\"Claude\"}],\"album\":{\"id\":2,\"name\":\"Piano Works\",\"picUrl\":\"http://127.0.0.1/a.jpg\"}}}]}");
                    return resp;
                }
                resp.status = 404;
                resp.body = QByteArrayLiteral("{\"code\":404}");
                return resp;
            }
            // Simulate primary endpoints returning 404 so service must fall back
            if (req.path == QStringLiteral("/search/hot")) {
                resp.status = 200;
                resp.body = QByteArrayLiteral("{\"code\":200,\"result\":{\"hots\":[{\"first\":\"Ravel\",\"second\":1}]}}");
                return resp;
            }
            if (req.path == QStringLiteral("/top/playlist")) {
                resp.status = 200;
                resp.body = QByteArrayLiteral("{\"code\":200,\"playlists\":[{\"id\":654321,\"name\":\"Top Playlist Fallback\",\"coverImgUrl\":\"http://127.0.0.1/fb.jpg\",\"trackCount\":40,\"creator\":{\"nickname\":\"Curator\"}}]}");
                return resp;
            }
            if (req.path == QStringLiteral("/top/song")) {
                resp.status = 200;
                resp.body = QByteArrayLiteral("{\"code\":200,\"data\":[{\"id\":9002,\"name\":\"Pavane\",\"duration\":240000,\"artists\":[{\"id\":3,\"name\":\"Maurice\"}],\"album\":{\"id\":4,\"name\":\"Collected\",\"picUrl\":\"http://127.0.0.1/p.jpg\"}}]}");
                return resp;
            }
            resp.status = 404;
            resp.body = QByteArrayLiteral("{\"code\":404}");
            return resp;
        });
        QVERIFY(server.listen());

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AppSettings settings(dir.filePath(QStringLiteral("settings.json")));
        StreamCacheManager cache(dir.filePath(QStringLiteral("cache")));
        OnlineMusicService service(&settings, &cache);
        PlaybackEngine engine(std::make_unique<MockAudioOutput>());
        ThemeManager theme(&settings);
        PlaybackController player(&engine, &settings, &theme);
        OnlineMusicController ctrl(&service, &cache, &settings, &player, nullptr, nullptr, nullptr);

        // With empty URL, ensureDiscoverLoaded must not issue any HTTP requests
        ctrl.ensureDiscoverLoaded();
        QCOMPARE(server.requestCount(QStringLiteral("/")), 0);
        QVERIFY(!ctrl.discoverLoaded());
        QVERIFY(!ctrl.discoverLoading());

        // Configure Netease URL -> ensureDiscoverLoaded populates from primary endpoints
        settings.setOnlineNeteaseApiUrl(server.baseUrl());
        ctrl.ensureDiscoverLoaded();
        QTRY_VERIFY_WITH_TIMEOUT(ctrl.discoverLoaded() && !ctrl.discoverLoading(), 4000);
        QCOMPARE(ctrl.hotSearches().size(), 1);
        QCOMPARE(ctrl.hotSearches().first().toMap().value(QStringLiteral("word")).toString(), QStringLiteral("Debussy"));
        auto* recMdl = qobject_cast<OnlinePlaylistListModel*>(ctrl.recommendedPlaylistsModel());
        auto* newMdl = qobject_cast<OnlineTrackListModel*>(ctrl.newSongsModel());
        QVERIFY(recMdl && newMdl);
        QCOMPARE(recMdl->count(), 1);
        QCOMPARE(newMdl->count(), 1);
        QCOMPARE(newMdl->tracks().first().title, QStringLiteral("Arabesque"));

        // Switch to fallback mode where primary endpoints 404 -> refreshDiscover succeeds via fallback endpoints
        useFallbackMode = true;
        ctrl.refreshDiscover();
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl.discoverLoading(), 4000);
        QVERIFY(ctrl.discoverLoaded());
        QVERIFY(ctrl.discoverError().isEmpty());
        QCOMPARE(ctrl.hotSearches().first().toMap().value(QStringLiteral("word")).toString(), QStringLiteral("Ravel"));
        QCOMPARE(recMdl->count(), 1);
        QCOMPARE(newMdl->tracks().first().title, QStringLiteral("Pavane"));

        // Memory hygiene: search -> switching category clears previous category model; opening/closing detail clears detail tracks
        ctrl.search(QStringLiteral("Debussy"), QStringLiteral("songs"));
        auto* songsMdl = qobject_cast<OnlineTrackListModel*>(ctrl.songsModel());
        auto* albumsMdl = qobject_cast<OnlineAlbumListModel*>(ctrl.albumsModel());
        QVERIFY(songsMdl && albumsMdl);

        useFallbackMode = false;
        ctrl.openAlbum(QStringLiteral("2"), QStringLiteral("Piano Works"), QStringLiteral("Claude"));
        auto* detailMdl = qobject_cast<OnlineTrackListModel*>(ctrl.detailTracksModel());
        QVERIFY(detailMdl != nullptr);
        ctrl.closeDetail();
        QVERIFY(!ctrl.inDetail());
        QCOMPARE(detailMdl->count(), 0);

        ctrl.search(QStringLiteral(""), QStringLiteral("songs"));
        ctrl.trimMemory();
        QCOMPARE(songsMdl->count(), 0);
        QCOMPARE(albumsMdl->count(), 0);

        // Seamless local-lossless upgrade resolution in OnlineMusicController
        LibraryDatabase db;
        QVERIFY(db.open(QStringLiteral(":memory:")));
        CoverArtProvider covers(dir.filePath(QStringLiteral("covers")));
        LibraryScanner scanner(&db, &covers);
        LibraryController libCtrl(&db, &scanner, &covers, &settings, &player);
        OnlineMusicController unifiedCtrl(&service, &cache, &settings, &player, &libCtrl, nullptr, &covers);

        const QString dlDir = dir.filePath(QStringLiteral("downloads"));
        QVERIFY(QDir().mkpath(dlDir));
        const QString dlWav = QDir(dlDir).filePath(QStringLiteral("9001.wav"));
        {
            // Minimal valid 44-byte WAV header + 0.1s PCM
            QFile wf(dlWav);
            QVERIFY(wf.open(QIODevice::WriteOnly));
            const uint32_t sr = 44100;
            const uint32_t dataBytes = 4410 * 2 * 2;
            const uint32_t riffSize = 36 + dataBytes;
            const uint32_t byteRate = sr * 4;
            QByteArray hdr;
            hdr.append("RIFF");
            hdr.append(reinterpret_cast<const char*>(&riffSize), 4);
            hdr.append("WAVEfmt ");
            const uint32_t fmtSize = 16;
            const uint16_t pcmFmt = 1, ch = 2, blkAlign = 4, bits = 16;
            hdr.append(reinterpret_cast<const char*>(&fmtSize), 4);
            hdr.append(reinterpret_cast<const char*>(&pcmFmt), 2);
            hdr.append(reinterpret_cast<const char*>(&ch), 2);
            hdr.append(reinterpret_cast<const char*>(&sr), 4);
            hdr.append(reinterpret_cast<const char*>(&byteRate), 4);
            hdr.append(reinterpret_cast<const char*>(&blkAlign), 2);
            hdr.append(reinterpret_cast<const char*>(&bits), 2);
            hdr.append("data");
            hdr.append(reinterpret_cast<const char*>(&dataBytes), 4);
            hdr.append(QByteArray(static_cast<int>(dataBytes), '\0'));
            wf.write(hdr);
        }

        // Favorite online track 9001, then ingest downloaded local file with upgrade
        QVariantMap onlineMap;
        onlineMap.insert(QStringLiteral("source"), QStringLiteral("netease"));
        onlineMap.insert(QStringLiteral("remoteId"), QStringLiteral("9001"));
        onlineMap.insert(QStringLiteral("title"), QStringLiteral("Arabesque"));
        onlineMap.insert(QStringLiteral("artist"), QStringLiteral("Claude"));
        onlineMap.insert(QStringLiteral("durationMs"), 100);
        QVERIFY(libCtrl.toggleTrackFavorite(onlineMap));
        QVERIFY(libCtrl.ingestDownloadedTrack(dlWav, QStringLiteral("netease"), QStringLiteral("9001")));

        // Discover newSongs on unifiedCtrl automatically reflects "done" downloadStatus and plays upgraded local track
        unifiedCtrl.refreshDiscover();
        QTRY_VERIFY_WITH_TIMEOUT(!unifiedCtrl.discoverLoading() && unifiedCtrl.discoverLoaded(), 4000);
        auto* uNewMdl = qobject_cast<OnlineTrackListModel*>(unifiedCtrl.newSongsModel());
        QVERIFY(uNewMdl != nullptr);
        QCOMPARE(uNewMdl->count(), 1);
        QCOMPARE(uNewMdl->data(uNewMdl->index(0), OnlineTrackListModel::DownloadStatusRole).toString(),
                 QStringLiteral("done"));
        unifiedCtrl.playNewSongAt(0);
        QTRY_VERIFY_WITH_TIMEOUT(!player.trackIsOnline() && player.trackFilePath().endsWith(QStringLiteral("9001.wav")), 3000);
        engine.stop();
    }
};

QTEST_MAIN(TestOnlineMusic)
#include "test_online_music.moc"
