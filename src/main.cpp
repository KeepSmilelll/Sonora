#include "audio/PlaybackEngine.h"
#include "core/AppSettings.h"
#include "core/MemoryStats.h"
#include "core/PlaybackController.h"
#include "core/ThemeManager.h"
#include "core/WindowLayout.h"
#include "library/CoverArtProvider.h"
#include "library/LibraryDatabase.h"
#include "library/LibraryScanner.h"
#include "lyrics/LyricsController.h"
#include "models/LibraryModels.h"
#include "online/LocalBackendManager.h"
#include "online/OnlineMusicController.h"
#include "online/OnlineMusicService.h"
#include "online/StreamCacheManager.h"
#include "platform/PlatformBridge.h"
#include "ui/EqCurveItem.h"
#include "ui/FluidAuroraItem.h"
#include "ui/IconImageProvider.h"
#include "ui/SpectrumVisualizerItem.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEvent>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QMouseEvent>
#include <QPixmapCache>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QScreen>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <iostream>
#include <optional>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace {

constexpr int kActivationTimeoutMs = 5000;
constexpr qint64 kMaxLaunchBytes = 1024 * 1024;

bool forwardLaunch(const QString& serverName, const QList<QUrl>& urls) {
    QJsonArray encodedUrls;
    for (const auto& url : urls) {
        encodedUrls.append(url.toString(QUrl::FullyEncoded));
    }
    const QByteArray request = QJsonDocument(encodedUrls).toJson(QJsonDocument::Compact) + '\n';
    if (request.size() > kMaxLaunchBytes) return false;

    QElapsedTimer timer;
    timer.start();
    // The first instance may still be between acquiring the lock and opening the socket.
    while (timer.elapsed() < kActivationTimeoutMs) {
        QLocalSocket socket;
        socket.connectToServer(serverName);
        if (socket.state() == QLocalSocket::ConnectedState || socket.waitForConnected(200)) {
            const auto remaining = [&]() {
                return qMax(0, kActivationTimeoutMs - static_cast<int>(timer.elapsed()));
            };
            if (socket.write(request) != request.size()) return false;
            while (socket.bytesToWrite() != 0) {
                if (!socket.waitForBytesWritten(remaining())) return false;
            }
            if (socket.bytesAvailable() == 0 && !socket.waitForReadyRead(remaining())) return false;
            return socket.read(1) == "1";
        }
        QThread::msleep(40);
    }
    return false;
}

void receiveLaunches(QLocalServer& server, QQuickWindow& window, virela::PlaybackController& player) {
    const auto acceptConnections = [&server, &window, &player]() {
        while (auto* socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(kActivationTimeoutMs, socket, &QLocalSocket::abort);
            socket->setReadBufferSize(kMaxLaunchBytes);
            const auto readRequest = [socket, &window, &player]() {
                if (!socket->canReadLine()) {
                    if (socket->bytesAvailable() >= kMaxLaunchBytes) socket->abort();
                    return;
                }
                const auto request = QJsonDocument::fromJson(socket->readLine(kMaxLaunchBytes + 1));
                if (!request.isArray()) {
                    socket->abort();
                    return;
                }

                QList<QUrl> urls;
                for (const auto& encoded : request.array()) {
                    if (!encoded.isString()) {
                        socket->abort();
                        return;
                    }
                    auto url = QUrl::fromEncoded(encoded.toString().toUtf8());
                    if (url.isValid() && !url.isEmpty()) urls.append(url);
                }
                QObject::disconnect(socket, nullptr, &window, nullptr);
                socket->write("1", 1);
                socket->disconnectFromServer();
                QMetaObject::invokeMethod(&window, "restoreMainWindow");
                if (!urls.isEmpty()) player.openUrls(urls, false);
            };
            QObject::connect(socket, &QLocalSocket::readyRead, &window, readRequest);
            readRequest();
        }
    };
    QObject::connect(&server, &QLocalServer::newConnection, &window, acceptConnections);
    acceptConnections();
}

class OutsideSearchFocusFilter final : public QObject {
public:
    explicit OutsideSearchFocusFilter(QQuickWindow* window) : QObject(window), window_(window) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::MouseButtonPress && window_ && window_->activeFocusItem()) {
            const auto* me = static_cast<const QMouseEvent*>(event);
            QMetaObject::invokeMethod(
                window_,
                "dismissSearchIfOutside",
                Q_ARG(QVariant, me->position().x()),
                Q_ARG(QVariant, me->position().y()));
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QQuickWindow* window_ = nullptr;
};

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Use the Direct3D 11 Qt Quick renderer on Windows.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc, argv);
    // Strict 4 MB QPixmapCache ceiling (down from Qt's 10 MB default) to cap startup RSS
    QPixmapCache::setCacheLimit(4096);
    app.setApplicationName(QStringLiteral("Virela"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("Virela"));
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/virela.ico")));
#ifdef _WIN32
    // Manage application exit explicitly so minimizing to system tray or toggling Mini Mode
    // never terminates the background player unexpectedly.
    app.setQuitOnLastWindowClosed(false);
#endif

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption smokeOpt(QStringLiteral("smoke-test-exit"),
                                QStringLiteral("Load QML UI, report memory stats, and exit cleanly."));
    parser.addOption(smokeOpt);
    parser.process(app);
    std::optional<QTemporaryDir> smokeProfile;
    if (parser.isSet(smokeOpt)) {
        // A running player must not intercept the startup smoke test.
        QStandardPaths::setTestModeEnabled(true);
        smokeProfile.emplace(QDir::temp().filePath(QStringLiteral("Virela-smoke-XXXXXX")));
        if (!smokeProfile->isValid()) {
            qCritical() << "Could not create the Virela smoke profile:" << smokeProfile->errorString();
            return 1;
        }
    }

    QList<QUrl> launchUrls;
    for (const auto& argument : parser.positionalArguments()) {
        launchUrls.append(QUrl::fromUserInput(argument, QDir::currentPath()));
    }

    const QString dataDir = smokeProfile ? smokeProfile->path() : virela::AppSettings::defaultDataDir();
    const QDir profile(dataDir);
    const auto profileHash = QCryptographicHash::hash(profile.canonicalPath().toUtf8(),
                                                     QCryptographicHash::Sha256).toHex();
    const QString serverName = QStringLiteral("virela-") + QString::fromLatin1(profileHash);
    QLockFile instanceLock(profile.filePath(QStringLiteral("instance.lock")));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(0)) {
        if (instanceLock.error() != QLockFile::LockFailedError) {
            qCritical("Could not lock the Virela profile.");
            return 1;
        }
#ifdef Q_OS_WIN
        qint64 primaryPid = 0;
        if (instanceLock.getLockInfo(&primaryPid, nullptr, nullptr)) {
            AllowSetForegroundWindow(static_cast<DWORD>(primaryPid));
        }
#endif
        if (forwardLaunch(serverName, launchUrls)) return 0;
        if (!instanceLock.tryLock(0)) {
            qWarning("The running Virela instance did not respond.");
            return 1;
        }
    }

    QLocalServer instanceServer;
    instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(serverName);
    if (!instanceServer.listen(serverName)) {
        qCritical() << "Could not listen for Virela launches:" << instanceServer.errorString();
        return 1;
    }

    virela::AppSettings settings(profile.filePath(QStringLiteral("settings.json")));
    auto platform = virela::PlatformBridge::create();

    virela::LibraryDatabase libraryDb;
    libraryDb.open(QDir(dataDir).filePath(QStringLiteral("library.db")));

    // CoverArtProvider ownership transfers to QQmlApplicationEngine via addImageProvider()
    auto* coverProvider = new virela::CoverArtProvider(
        QDir(dataDir).filePath(QStringLiteral("cache/covers")),
        virela::CoverArtProvider::kDefaultRamCacheBytes);

    virela::LibraryScanner libraryScanner(&libraryDb, coverProvider);
    virela::StreamCacheManager streamCache(
        profile.filePath(QStringLiteral("cache/streams")),
        static_cast<qint64>(settings.onlineCacheMaxMb()) * 1024LL * 1024LL);
    virela::OnlineMusicService onlineService(&settings, &streamCache);
    QObject::connect(&settings, &virela::AppSettings::onlineSettingsChanged, &streamCache, [&]() {
        streamCache.setMaxCacheBytes(static_cast<qint64>(settings.onlineCacheMaxMb()) * 1024LL * 1024LL);
    });

    virela::PlaybackEngine audioEngine;
    audioEngine.setOnlineStreamResolver([&streamCache, &onlineService](
        const virela::TrackInfo& track, const std::atomic<bool>* abortFlag) -> virela::ResolvedOnlinePlayback {
        virela::ResolvedOnlinePlayback out;
        const QString quality = onlineService.currentQuality();
        const QString cacheKey = virela::StreamCacheManager::makeCacheKey(track.source, track.remoteId, quality);

        const QString cachedFile = streamCache.findCompletedCacheFile(cacheKey);
        const bool looksLikeStaleTrial = !cachedFile.isEmpty()
            && track.durationMs >= 90000
            && QFileInfo(cachedFile).size() < 850000;
        if (!cachedFile.isEmpty() && !looksLikeStaleTrial) {
            out.localCachePath = cachedFile;
            out.formatBadge = virela::OnlineMusicService::formatBadgeForQuality(quality);
            return out;
        }

        if (!looksLikeStaleTrial) {
            if (auto existing = streamCache.activeSessionForKey(cacheKey)) {
                out.session = std::move(existing);
                out.formatBadge = virela::OnlineMusicService::formatBadgeForQuality(quality);
                return out;
            }
        }

        const virela::ResolvedStreamInfo resolved = onlineService.resolveStreamBlocking(track, quality, abortFlag);
        if (abortFlag && abortFlag->load(std::memory_order_acquire)) {
            return out;
        }
        if (!resolved.ok || resolved.streamUrl.isEmpty()) {
            out.error = resolved.error.isEmpty()
                ? QStringLiteral("Failed to resolve online stream URL.")
                : resolved.error;
            return out;
        }

        const QString effectiveKey = resolved.isTrial ? (cacheKey + QStringLiteral("_trial")) : cacheKey;
        const QString streamCookie = (resolved.backendUsed == QStringLiteral("netease"))
            ? onlineService.currentCookie()
            : QString();
        out.session = streamCache.openOrStartSession(
            effectiveKey, QUrl(resolved.streamUrl), streamCookie, resolved.codecHint);
        out.formatBadge = resolved.formatBadge;
        out.codec = resolved.codecHint;
        out.bitrateKbps = resolved.bitrateKbps;
        return out;
    });

    virela::ThemeManager themeManager(&settings);
    virela::PlaybackController playerController(&audioEngine, &settings, &themeManager, platform.get());
    playerController.setCoverLoader([coverProvider](const QString& hash) {
        return coverProvider->loadCover(hash, 360);
    });
    virela::LibraryController libraryController(&libraryDb, &libraryScanner, coverProvider, &settings, &playerController);
    virela::LyricsController lyricsController;
    virela::LocalBackendManager localBackends(&settings);
    virela::OnlineMusicController onlineController(
        &onlineService, &streamCache, &settings,
        &playerController, &libraryController, &lyricsController, coverProvider);
    onlineController.setLocalBackendManager(&localBackends);

    if (!parser.isSet(smokeOpt)) {
        localBackends.setLifecycleActive(true);
        localBackends.startConfiguredServices();
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &localBackends, [&localBackends]() {
        localBackends.stopAllServices();
    });

    // Connect playback clock & track changes -> OnlineMusicController / LyricsController
    QObject::connect(&audioEngine, &virela::PlaybackEngine::currentTrackChanged,
                     &onlineController, [&](const virela::TrackInfo& track, int) {
        onlineController.onCurrentTrackChanged(track);
    });
    QObject::connect(&audioEngine, &virela::PlaybackEngine::positionChanged,
                     &lyricsController, [&](qint64 posMs) {
        lyricsController.updatePositionMs(posMs);
    });

    // Bind custom single-draw-call GPU SceneGraph items to PlaybackEngine
    virela::SpectrumVisualizerItem::setSharedEngine(&audioEngine);
    virela::EqCurveItem::setSharedEngine(&audioEngine);
    qmlRegisterType<virela::SpectrumVisualizerItem>("Virela.UI", 1, 0, "SpectrumVisualizerItem");
    qmlRegisterType<virela::EqCurveItem>("Virela.UI", 1, 0, "EqCurveItem");
    qmlRegisterType<virela::FluidAuroraItem>("Virela.UI", 1, 0, "FluidAuroraItem");

    QQmlApplicationEngine qmlEngine;
    qmlEngine.addImageProvider(QStringLiteral("cover"), coverProvider);
    qmlEngine.addImageProvider(QStringLiteral("icon"), new virela::IconImageProvider());
    themeManager.setQmlEngine(&qmlEngine);

    qmlEngine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &settings);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("player"), &playerController);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("library"), &libraryController);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("lyricsController"), &lyricsController);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("onlineMusic"), &onlineController);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("platformBridge"), platform.get());

    QObject::connect(
        &qmlEngine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    qmlEngine.loadFromModule(QStringLiteral("Virela.UI"), QStringLiteral("Main"));

    auto* mainWin = qmlEngine.rootObjects().isEmpty()
        ? nullptr : qobject_cast<QQuickWindow*>(qmlEngine.rootObjects().constFirst());
    if (mainWin) {
        QScreen* scr = mainWin->screen() ? mainWin->screen() : QGuiApplication::primaryScreen();
        const auto initial = virela::computeInitialWindowGeometry(
            scr ? scr->availableGeometry() : QRect(0, 0, 1280, 720));
        mainWin->setMinimumSize(initial.minimumSize);
        mainWin->setGeometry(initial.geometry);
        platform->attachMainWindow(mainWin, 46);
        mainWin->installEventFilter(new OutsideSearchFocusFilter(mainWin));
        platform->setWindowBackdrop(mainWin, themeManager.backdropMode(), themeManager.isDark());
        mainWin->show();
        platform->initMediaSession(mainWin);
        platform->initTray(QStringLiteral("Virela"));
        platform->setGlobalHotkeysEnabled(settings.globalHotkeysEnabled());

        auto updateBackdrop = [mainWin, &platform, &themeManager]() {
            platform->setWindowBackdrop(mainWin, themeManager.backdropMode(), themeManager.isDark());
        };
        QObject::connect(&themeManager, &virela::ThemeManager::themeChanged, mainWin, updateBackdrop);
        QObject::connect(&themeManager, &virela::ThemeManager::backdropChanged, mainWin, updateBackdrop);
        QObject::connect(platform.get(), &virela::PlatformBridge::quitRequested, &app,
                         [mainWin, &platform]() {
            platform->markQuitting();
            platform->shutdownTray();
            if (mainWin) {
                mainWin->setProperty("forceQuit", true);
                mainWin->setProperty("miniModeOpen", false);
                mainWin->close();
            }
            QCoreApplication::exit(0);
        }, Qt::QueuedConnection);

        receiveLaunches(instanceServer, *mainWin, playerController);

        // Reclaim transient QML/cover caches & trim Win32 working set when minimized to tray
        QObject::connect(mainWin, &QWindow::visibleChanged, mainWin,
                         [mainWin, coverProvider, &qmlEngine, &platform](bool isVisible) {
            if (isVisible || platform->isQuitting()) return;
            QTimer::singleShot(350, mainWin, [mainWin, coverProvider, &qmlEngine, &platform]() {
                if (!mainWin->isVisible() && !platform->isQuitting()) {
                    coverProvider->clearRamCache();
                    qmlEngine.trimComponentCache();
                    qmlEngine.collectGarbage();
#ifdef Q_OS_WIN
                    SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));
#endif
                }
            });
        });
    }

    if (!launchUrls.isEmpty()) {
        playerController.openUrls(launchUrls, false);
    }

    if (parser.isSet(smokeOpt)) {
        QTimer::singleShot(150, &app, [mainWin, &platform]() {
            if (!mainWin) {
                QCoreApplication::exit(1);
                return;
            }
            const auto mem = virela::queryProcessMemory();
            std::cout << "[SMOKE] Virela Full App loaded cleanly. Window="
                      << mainWin->width() << 'x' << mainWin->height()
                      << " @" << mainWin->devicePixelRatio() << "x, RSS="
                      << mem.workingSetMb << " MB, Private="
                      << mem.privateBytesMb << " MB" << std::endl;
            platform->requestQuit();
        });
    }

    const int rc = app.exec();
    platform->shutdownTray();
    libraryScanner.cancelScan();
    return rc;
}
