#pragma once

#include <QtGlobal>
#if defined(_WIN32) || defined(Q_OS_WIN)

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <cstdint>

class QWindow;

namespace sonora {

class Win32GlobalHotkey final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit Win32GlobalHotkey(QObject* parent = nullptr);
    ~Win32GlobalHotkey() override;

    void attachWindow(QWindow* window);
    void setEnabled(bool enabled);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void togglePlayPauseRequested();
    void nextRequested();
    void previousRequested();
    void volumeUpRequested();
    void volumeDownRequested();
    void toggleMiniModeRequested();

private:
    void registerAll();
    void unregisterAll();

    uintptr_t hwnd_ = 0;
    bool enabled_ = false;
    bool registered_ = false;
    bool filterInstalled_ = false;
};

} // namespace sonora

#endif // _WIN32
