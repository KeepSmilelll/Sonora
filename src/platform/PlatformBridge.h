#pragma once

#include "audio/AudioTypes.h"

#include <QImage>
#include <QObject>
#include <QRectF>
#include <QString>
#include <QVariantList>
#include <QVector>
#include <memory>

class QWindow;

namespace virela {

class PlatformBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool quitting READ isQuitting NOTIFY quittingChanged)

public:
    explicit PlatformBridge(QObject* parent = nullptr) : QObject(parent) {}
    ~PlatformBridge() override = default;

    bool isQuitting() const noexcept { return quitting_; }
    void markQuitting() {
        if (!quitting_) {
            quitting_ = true;
            emit quittingChanged();
        }
    }
    Q_INVOKABLE void requestQuit() {
        markQuitting();
        emit quitRequested();
    }

    // Attach native frameless window handling for Aero Snap, shadows, and window chrome.
    virtual void attachMainWindow(QWindow* window, int titleBarHeightPx = 42) = 0;
    virtual void setWindowBackdrop(QWindow* window, const QString& backdropMode, bool isDark) = 0;
    virtual void setTitleBarHeight(int heightPx) = 0;
    Q_INVOKABLE virtual void setTitleBarDragEnabled(bool enabled) { Q_UNUSED(enabled); }
    Q_INVOKABLE virtual void setTitleBarInteractiveRects(const QVariantList& rects) { Q_UNUSED(rects); }
    Q_INVOKABLE virtual void toggleMaximize(QWindow* window);

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
    void quittingChanged();

private:
    bool quitting_ = false;
};

} // namespace virela
