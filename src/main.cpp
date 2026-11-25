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
#include "ui/SpectrumVisualizerItem.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <iostream>

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

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption smokeOpt(QStringLiteral("smoke-test-exit"),
                                QStringLiteral("Load QML UI, report memory stats, and exit cleanly."));
    parser.addOption(smokeOpt);
    parser.process(app);

    const QString dataDir = sonora::AppSettings::defaultDataDir();
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
            platform->attachMainWindow(mainWin, 44);
            platform->setWindowBackdrop(mainWin, themeManager.backdropMode(), themeManager.isDark());
            platform->initMediaSession(mainWin);
            platform->initTray(QStringLiteral("Sonora"));
            platform->setGlobalHotkeysEnabled(settings.globalHotkeysEnabled());

            auto updateBackdrop = [mainWin, &platform, &themeManager]() {
                platform->setWindowBackdrop(mainWin, themeManager.backdropMode(), themeManager.isDark());
            };
            QObject::connect(&themeManager, &sonora::ThemeManager::themeChanged, mainWin, updateBackdrop);
            QObject::connect(&themeManager, &sonora::ThemeManager::backdropChanged, mainWin, updateBackdrop);
            QObject::connect(platform.get(), &sonora::PlatformBridge::quitRequested, &app, &QCoreApplication::quit);
        }
    }

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        QList<QUrl> urls;
        for (const QString& p : positional) {
            urls.append(QUrl::fromUserInput(p));
        }
        playerController.openUrls(urls, false);
    }

    if (parser.isSet(smokeOpt)) {
        QTimer::singleShot(150, &app, [&]() {
            const auto mem = sonora::queryProcessMemory();
            std::cout << "[SMOKE] Sonora Full App loaded cleanly. RSS="
                      << mem.workingSetMb << " MB, Private="
                      << mem.privateBytesMb << " MB" << std::endl;
            app.quit();
        });
    }

    const int rc = app.exec();
    platform->shutdownTray();
    libraryScanner.cancelScan();
    return rc;
}
