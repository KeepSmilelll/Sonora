#include "audio/PlaybackEngine.h"
#include "core/AppSettings.h"
#include "core/MemoryStats.h"
#include "core/PlaybackController.h"
#include "core/ThemeManager.h"
#include "library/CoverArtProvider.h"
#include "library/LibraryDatabase.h"
#include "library/LibraryScanner.h"
#include "lyrics/LyricsController.h"
#include "models/LibraryModels.h"
#include "platform/PlatformBridge.h"
#include "ui/EqCurveItem.h"
#include "ui/FluidAuroraItem.h"
#include "ui/SpectrumVisualizerItem.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <iostream>

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

void receiveLaunches(QLocalServer& server, QQuickWindow& window, sonora::PlaybackController& player) {
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

} // namespace

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Direct3D 11 QRhi on Windows 10/11 for native DWM Mica compositing & low RAM
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Sonora"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));
    app.setOrganizationName(QStringLiteral("Sonora"));
    app.setWindowIcon(QIcon(QStringLiteral(":/resources/sonora.ico")));
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

    QList<QUrl> launchUrls;
    for (const auto& argument : parser.positionalArguments()) {
        launchUrls.append(QUrl::fromUserInput(argument, QDir::currentPath()));
    }

    const QString dataDir = sonora::AppSettings::defaultDataDir();
    const QDir profile(dataDir);
    const auto profileHash = QCryptographicHash::hash(profile.canonicalPath().toUtf8(),
                                                     QCryptographicHash::Sha256).toHex();
    const QString serverName = QStringLiteral("sonora-") + QString::fromLatin1(profileHash);
    QLockFile instanceLock(profile.filePath(QStringLiteral("instance.lock")));
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(0)) {
        if (instanceLock.error() != QLockFile::LockFailedError) {
            qCritical("Could not lock the Sonora profile.");
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
            qWarning("The running Sonora instance did not respond.");
            return 1;
        }
    }

    QLocalServer instanceServer;
    instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
    QLocalServer::removeServer(serverName);
    if (!instanceServer.listen(serverName)) {
        qCritical() << "Could not listen for Sonora launches:" << instanceServer.errorString();
        return 1;
    }

    sonora::AppSettings settings;
    auto platform = sonora::PlatformBridge::create();

    sonora::LibraryDatabase libraryDb;
    libraryDb.open(QDir(dataDir).filePath(QStringLiteral("library.db")));

    // CoverArtProvider ownership transfers to QQmlApplicationEngine via addImageProvider()
    auto* coverProvider = new sonora::CoverArtProvider(
        QDir(dataDir).filePath(QStringLiteral("cache/covers")),
        sonora::CoverArtProvider::kDefaultRamCacheBytes);

    sonora::LibraryScanner libraryScanner(&libraryDb, coverProvider);
    sonora::PlaybackEngine audioEngine;
    sonora::ThemeManager themeManager(&settings);
    sonora::PlaybackController playerController(&audioEngine, &settings, &themeManager, platform.get());
    playerController.setCoverLoader([coverProvider](const QString& hash) {
        return coverProvider->loadCover(hash, 360);
    });
    sonora::LibraryController libraryController(&libraryDb, &libraryScanner, coverProvider, &settings, &playerController);
    sonora::LyricsController lyricsController;

    // Connect playback clock & track changes -> LyricsController
    QObject::connect(&audioEngine, &sonora::PlaybackEngine::currentTrackChanged,
                     &lyricsController, [&](const sonora::TrackInfo& track, int) {
        lyricsController.loadForTrack(track.filePath, track.embeddedLyrics);
    });
    QObject::connect(&audioEngine, &sonora::PlaybackEngine::positionChanged,
                     &lyricsController, [&](qint64 posMs) {
        lyricsController.updatePositionMs(posMs);
    });

    // Bind custom single-draw-call GPU SceneGraph items to PlaybackEngine
    sonora::SpectrumVisualizerItem::setSharedEngine(&audioEngine);
    sonora::EqCurveItem::setSharedEngine(&audioEngine);
    qmlRegisterType<sonora::SpectrumVisualizerItem>("Sonora.UI", 1, 0, "SpectrumVisualizerItem");
    qmlRegisterType<sonora::EqCurveItem>("Sonora.UI", 1, 0, "EqCurveItem");
    qmlRegisterType<sonora::FluidAuroraItem>("Sonora.UI", 1, 0, "FluidAuroraItem");

    QQmlApplicationEngine qmlEngine;
    qmlEngine.addImageProvider(QStringLiteral("cover"), coverProvider);
    themeManager.setQmlEngine(&qmlEngine);

    qmlEngine.rootContext()->setContextProperty(QStringLiteral("appSettings"), &settings);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("themeManager"), &themeManager);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("player"), &playerController);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("library"), &libraryController);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("lyricsController"), &lyricsController);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("platformBridge"), platform.get());

    QObject::connect(
        &qmlEngine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(1); },
        Qt::QueuedConnection);

    qmlEngine.loadFromModule(QStringLiteral("Sonora.UI"), QStringLiteral("Main"));

    if (!qmlEngine.rootObjects().isEmpty()) {
        if (auto* mainWin = qobject_cast<QQuickWindow*>(qmlEngine.rootObjects().constFirst())) {
            platform->attachMainWindow(mainWin, 46);
            platform->setWindowBackdrop(mainWin, themeManager.backdropMode(), themeManager.isDark());
            mainWin->show();
            platform->initMediaSession(mainWin);
            platform->initTray(QStringLiteral("Sonora"));
            platform->setGlobalHotkeysEnabled(settings.globalHotkeysEnabled());

            auto updateBackdrop = [mainWin, &platform, &themeManager]() {
                platform->setWindowBackdrop(mainWin, themeManager.backdropMode(), themeManager.isDark());
            };
            QObject::connect(&themeManager, &sonora::ThemeManager::themeChanged, mainWin, updateBackdrop);
            QObject::connect(&themeManager, &sonora::ThemeManager::backdropChanged, mainWin, updateBackdrop);
            QObject::connect(platform.get(), &sonora::PlatformBridge::quitRequested, &app,
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
        }
    }

    if (!launchUrls.isEmpty()) {
        playerController.openUrls(launchUrls, false);
    }

    if (parser.isSet(smokeOpt)) {
        QTimer::singleShot(150, &app, [&]() {
            const auto mem = sonora::queryProcessMemory();
            std::cout << "[SMOKE] Sonora Full App loaded cleanly. RSS="
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
