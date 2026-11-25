#pragma once

#ifdef _WIN32

#include <QAbstractNativeEventFilter>
#include <QString>
#include <cstdint>

class QWindow;

namespace sonora {

class Win32WindowHelper final : public QAbstractNativeEventFilter {
public:
    Win32WindowHelper() = default;
    ~Win32WindowHelper() override;

    void attach(QWindow* window, int titleBarHeightPx = 42);
    void setTitleBarHeight(int heightPx) { titleBarHeightPx_ = heightPx; }
    void setBackdrop(QWindow* window, const QString& backdropMode, bool isDark);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    QWindow* window_ = nullptr;
    uintptr_t hwnd_ = 0;
    int titleBarHeightPx_ = 42;
    bool installed_ = false;
};

} // namespace sonora

#endif // _WIN32
