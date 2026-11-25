#include "platform/PlatformBridge.h"

#ifdef _WIN32
#include "platform/win32/Win32WindowHelper.h"
#include "platform/win32/Win32SmtcController.h"
#include "platform/win32/Win32TrayIcon.h"
#include "platform/win32/Win32GlobalHotkey.h"

namespace sonora {

class Win32PlatformBridge final : public PlatformBridge {
public:
    explicit Win32PlatformBridge(QObject* parent = nullptr) : PlatformBridge(parent) {
        connect(&smtc_, &Win32SmtcController::playRequested, this, &PlatformBridge::mediaPlayRequested);
        connect(&smtc_, &Win32SmtcController::pauseRequested, this, &PlatformBridge::mediaPauseRequested);
        connect(&smtc_, &Win32SmtcController::stopRequested, this, &PlatformBridge::mediaStopRequested);
        connect(&smtc_, &Win32SmtcController::nextRequested, this, &PlatformBridge::mediaNextRequested);
        connect(&smtc_, &Win32SmtcController::previousRequested, this, &PlatformBridge::mediaPreviousRequested);

        connect(&tray_, &Win32TrayIcon::togglePlayPauseRequested, this, &PlatformBridge::mediaTogglePlayPauseRequested);
        connect(&tray_, &Win32TrayIcon::nextRequested, this, &PlatformBridge::mediaNextRequested);
        connect(&tray_, &Win32TrayIcon::previousRequested, this, &PlatformBridge::mediaPreviousRequested);
        connect(&tray_, &Win32TrayIcon::toggleMiniModeRequested, this, &PlatformBridge::toggleMiniModeRequested);
        connect(&tray_, &Win32TrayIcon::showWindowRequested, this, &PlatformBridge::showMainWindowRequested);
        connect(&tray_, &Win32TrayIcon::quitRequested, this, &PlatformBridge::quitRequested);

        connect(&hotkeys_, &Win32GlobalHotkey::togglePlayPauseRequested, this, &PlatformBridge::mediaTogglePlayPauseRequested);
        connect(&hotkeys_, &Win32GlobalHotkey::nextRequested, this, &PlatformBridge::mediaNextRequested);
        connect(&hotkeys_, &Win32GlobalHotkey::previousRequested, this, &PlatformBridge::mediaPreviousRequested);
        connect(&hotkeys_, &Win32GlobalHotkey::volumeUpRequested, this, &PlatformBridge::mediaVolumeUpRequested);
        connect(&hotkeys_, &Win32GlobalHotkey::volumeDownRequested, this, &PlatformBridge::mediaVolumeDownRequested);
        connect(&hotkeys_, &Win32GlobalHotkey::toggleMiniModeRequested, this, &PlatformBridge::toggleMiniModeRequested);
    }

    void attachMainWindow(QWindow* window, int titleBarHeightPx) override {
        windowHelper_.attach(window, titleBarHeightPx);
        tray_.attachWindow(window);
        hotkeys_.attachWindow(window);
    }

    void setWindowBackdrop(QWindow* window, const QString& backdropMode, bool isDark) override {
        windowHelper_.setBackdrop(window, backdropMode, isDark);
    }

    void setTitleBarHeight(int heightPx) override {
        windowHelper_.setTitleBarHeight(heightPx);
    }

    void initTray(const QString& tooltip) override { tray_.init(tooltip); }
    void updateTrayTooltip(const QString& tooltip) override { tray_.updateTooltip(tooltip); }
    void setTrayPlayingState(bool playing) override { tray_.setPlayingState(playing); }
    void shutdownTray() override { tray_.shutdown(); }

    void initMediaSession(QWindow* window) override { smtc_.init(window); }
    void updateMediaSessionTrack(const TrackInfo& track, const QImage& thumbnail) override {
        smtc_.updateTrack(track, thumbnail);
    }
    void updateMediaSessionState(PlaybackState state, int64_t posMs, int64_t durMs) override {
        smtc_.updatePlaybackState(state, posMs, durMs);
    }

    void setGlobalHotkeysEnabled(bool enabled) override {
        hotkeys_.setEnabled(enabled);
    }

private:
    Win32WindowHelper windowHelper_;
    Win32SmtcController smtc_;
    Win32TrayIcon tray_;
    Win32GlobalHotkey hotkeys_;
};

std::unique_ptr<PlatformBridge> PlatformBridge::create(QObject* parent) {
    return std::make_unique<Win32PlatformBridge>(parent);
}

} // namespace sonora

#else

namespace sonora {

class StubPlatformBridge final : public PlatformBridge {
public:
    explicit StubPlatformBridge(QObject* parent = nullptr) : PlatformBridge(parent) {}

    void attachMainWindow(QWindow* /*window*/, int /*titleBarHeightPx*/) override {}
    void setWindowBackdrop(QWindow* /*window*/, const QString& /*backdropMode*/, bool /*isDark*/) override {}
    void setTitleBarHeight(int /*heightPx*/) override {}
};

std::unique_ptr<PlatformBridge> PlatformBridge::create(QObject* parent) {
    return std::make_unique<StubPlatformBridge>(parent);
}

} // namespace sonora

#endif
