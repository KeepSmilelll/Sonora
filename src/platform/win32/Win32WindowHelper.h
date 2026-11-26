#pragma once

#include <QtGlobal>
#if defined(_WIN32) || defined(Q_OS_WIN)

#include <QAbstractNativeEventFilter>
#include <QRectF>
#include <QString>
#include <QVector>
#include <cstdint>

class QWindow;

namespace sonora {

class Win32WindowHelper final : public QAbstractNativeEventFilter {
public:
    Win32WindowHelper() = default;
    ~Win32WindowHelper() override;

    void attach(QWindow* window, int titleBarHeightPx = 42);
    void setTitleBarHeight(int heightPx) { titleBarHeightPx_ = heightPx; }
    void setTitleBarDragEnabled(bool enabled) { titleBarDragEnabled_ = enabled; }
    void setInteractiveRects(const QVector<QRectF>& rects) { interactiveRects_ = rects; }
    void setBackdrop(QWindow* window, const QString& backdropMode, bool isDark);
    void toggleMaximize(QWindow* window);

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void applyWindowStyle();

    QWindow* window_ = nullptr;
    uintptr_t hwnd_ = 0;
    int titleBarHeightPx_ = 42;
    bool installed_ = false;
    bool titleBarDragEnabled_ = true;
    QVector<QRectF> interactiveRects_;
};

} // namespace sonora

#endif // _WIN32
