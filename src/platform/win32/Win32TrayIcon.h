#pragma once

#ifdef _WIN32

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>
#include <cstdint>

class QWindow;

namespace sonora {

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
    void showContextMenu();

    uintptr_t hwnd_ = 0;
    bool active_ = false;
    bool playing_ = false;
    bool filterInstalled_ = false;
    QString tooltip_ = QStringLiteral("Sonora");
};

} // namespace sonora

#endif // _WIN32
