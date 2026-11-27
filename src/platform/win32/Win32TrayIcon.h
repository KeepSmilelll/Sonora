#pragma once

#include <QtGlobal>
#if defined(_WIN32) || defined(Q_OS_WIN)

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>
#include <cstdint>

class QWindow;

namespace virela {

// Zero-QtWidgets Windows system tray icon implemented directly via Shell_NotifyIconW.
class Win32TrayIcon final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit Win32TrayIcon(QObject* parent = nullptr);
    ~Win32TrayIcon() override;

    void attachWindow(QWindow* window);
    void init(const QString& tooltip);
    void updateTooltip(const QString& tooltip);
    void setPlayingState(bool playing);
    void shutdown();

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void togglePlayPauseRequested();
    void nextRequested();
    void previousRequested();
    void toggleMiniModeRequested();
    void showWindowRequested();
    void quitRequested();

private:
    bool addTrayIcon();
    void showContextMenu();

    uintptr_t hwnd_ = 0;
    uint32_t taskbarCreatedMsg_ = 0;
    bool active_ = false;
    bool playing_ = false;
    bool filterInstalled_ = false;
    bool menuOpen_ = false;
    QString tooltip_ = QStringLiteral("Virela");
};

} // namespace virela

#endif // _WIN32
