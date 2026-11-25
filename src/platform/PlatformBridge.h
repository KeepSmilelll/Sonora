#pragma once

#include "audio/AudioTypes.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <memory>

class QWindow;

namespace sonora {

class PlatformBridge : public QObject {
    Q_OBJECT

public:
    explicit PlatformBridge(QObject* parent = nullptr) : QObject(parent) {}
    ~PlatformBridge() override = default;

    // Attaches native frameless window handling (Aero Snap, shadows, DWM Mica/Acrylic)
    virtual void attachMainWindow(QWindow* window, int titleBarHeightPx = 42) = 0;
    virtual void setWindowBackdrop(QWindow* window, const QString& backdropMode, bool isDark) = 0;
    virtual void setTitleBarHeight(int heightPx) = 0;

    // System Tray (Phase 3 Win32 Shell_NotifyIconW; no-op stub on Linux)
    virtual void initTray(const QString& tooltip) { Q_UNUSED(tooltip); }
    virtual void updateTrayTooltip(const QString& tooltip) { Q_UNUSED(tooltip); }
    virtual void setTrayPlayingState(bool playing) { Q_UNUSED(playing); }
    virtual void shutdownTray() {}

    // Windows SMTC Lock Screen & Media Keys (Phase 3 WinRT/COM interop)
    virtual void initMediaSession(QWindow* window) { Q_UNUSED(window); }
    virtual void updateMediaSessionTrack(const TrackInfo& track, const QImage& thumbnail) {
        Q_UNUSED(track);
        Q_UNUSED(thumbnail);
    }
    virtual void updateMediaSessionState(PlaybackState state, int64_t posMs, int64_t durMs) {
        Q_UNUSED(state);
        Q_UNUSED(posMs);
        Q_UNUSED(durMs);
    }

    // Global Hotkeys (Phase 3 Win32 RegisterHotKey)
    virtual void setGlobalHotkeysEnabled(bool enabled) { Q_UNUSED(enabled); }

    static std::unique_ptr<PlatformBridge> create(QObject* parent = nullptr);

signals:
    void mediaPlayRequested();
    void mediaPauseRequested();
    void mediaTogglePlayPauseRequested();
    void mediaStopRequested();
    void mediaNextRequested();
    void mediaPreviousRequested();
    void mediaVolumeUpRequested();
    void mediaVolumeDownRequested();
    void toggleMiniModeRequested();
    void showMainWindowRequested();
    void quitRequested();
};

} // namespace sonora
