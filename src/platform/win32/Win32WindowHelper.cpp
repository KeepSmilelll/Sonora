#ifdef _WIN32

#include "platform/win32/Win32WindowHelper.h"

#include <QCoreApplication>
#include <QWindow>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <cmath>

namespace sonora {

namespace {

constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
constexpr DWORD kDwmwaWindowCornerPreference = 33;
constexpr DWORD kDwmwaSystemBackdropType = 38;
constexpr DWORD kDwmsbtNone = 1;
constexpr DWORD kDwmsbtMainWindow = 2;
constexpr DWORD kDwmsbtTransientWindow = 3;

bool isWindows11OrNewer() {
    using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto fn = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (!fn) return false;
    OSVERSIONINFOW rovi{};
    rovi.dwOSVersionInfoSize = sizeof(rovi);
    return fn(&rovi) == 0 && rovi.dwMajorVersion >= 10 && rovi.dwBuildNumber >= 22621;
}

UINT windowDpi(HWND hwnd) {
    using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
    static const auto fn = reinterpret_cast<GetDpiForWindowFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (fn && hwnd) {
        const UINT dpi = fn(hwnd);
        if (dpi > 0) return dpi;
    }
    return 96;
}

void adjustForAutoHideTaskbar(HMONITOR monitor, RECT& rect) {
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    const UINT state = static_cast<UINT>(SHAppBarMessage(ABM_GETSTATE, &abd));
    if ((state & ABS_AUTOHIDE) == 0) {
        return;
    }
    if (!SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
        return;
    }
    if (MonitorFromRect(&abd.rc, MONITOR_DEFAULTTONEAREST) != monitor) {
        return;
    }
    switch (abd.uEdge) {
    case ABE_LEFT:   rect.left += 1; break;
    case ABE_TOP:    rect.top += 1; break;
    case ABE_RIGHT:  rect.right -= 1; break;
    case ABE_BOTTOM: rect.bottom -= 1; break;
    default: break;
    }
}

} // namespace

Win32WindowHelper::~Win32WindowHelper() {
    if (installed_ && QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
        installed_ = false;
    }
}

void Win32WindowHelper::attach(QWindow* window, int titleBarHeightPx) {
    if (!window) return;
    window_ = window;
    titleBarHeightPx_ = titleBarHeightPx;
    hwnd_ = static_cast<uintptr_t>(window->winId());
    if (!hwnd_) return;

    // Install native event filter BEFORE modifying GWL_STYLE or calling SetWindowPos(SWP_FRAMECHANGED)
    // so the very first synchronous WM_NCCALCSIZE is intercepted and strips the native title bar.
    if (!installed_ && QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
        installed_ = true;
    }

    applyWindowStyle();
}

void Win32WindowHelper::applyWindowStyle() {
    if (!hwnd_) return;
    HWND hwnd = reinterpret_cast<HWND>(hwnd_);

    // Replace Qt::FramelessWindowHint's WS_POPUP with overlapped resize/snap styles,
    // and strip WS_SYSMENU so Windows 11 DWM never paints native caption buttons behind
    // the translucent Mica/Acrylic custom TitleBar.
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style &= ~static_cast<LONG_PTR>(WS_POPUP | WS_SYSMENU);
    style |= WS_OVERLAPPED | WS_THICKFRAME | WS_CAPTION | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    // Extend DWM glass frame over the entire client area so Mica/Acrylic & shadow work seamlessly
    const MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Request Windows 11 rounded corners when supported
    const DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, kDwmwaWindowCornerPreference, &cornerPref, sizeof(cornerPref));

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void Win32WindowHelper::setBackdrop(QWindow* window, const QString& backdropMode, bool isDark) {
    if (!window) return;
    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window->winId()));
    if (!hwnd) return;

    const BOOL darkFlag = isDark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, kDwmwaUseImmersiveDarkMode, &darkFlag, sizeof(darkFlag));

    if (!isWindows11OrNewer()) {
        return;
    }

    DWORD backdrop = kDwmsbtNone;
    if (backdropMode == QStringLiteral("mica")) {
        backdrop = kDwmsbtMainWindow;
    } else if (backdropMode == QStringLiteral("acrylic")) {
        backdrop = kDwmsbtTransientWindow;
    }
    DwmSetWindowAttribute(hwnd, kDwmwaSystemBackdropType, &backdrop, sizeof(backdrop));
}

void Win32WindowHelper::toggleMaximize(QWindow* window) {
    HWND hwnd = window
        ? reinterpret_cast<HWND>(static_cast<uintptr_t>(window->winId()))
        : reinterpret_cast<HWND>(hwnd_);
    if (!hwnd) return;
    ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

bool Win32WindowHelper::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
        return false;
    }
    MSG* msg = static_cast<MSG*>(message);
    if (!msg || reinterpret_cast<uintptr_t>(msg->hwnd) != hwnd_) {
        return false;
    }

    switch (msg->message) {
    case WM_NCCALCSIZE: {
        if (msg->wParam == TRUE) {
            auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            if (params && IsZoomed(msg->hwnd)) {
                HMONITOR mon = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{};
                mi.cbSize = sizeof(mi);
                if (mon && GetMonitorInfoW(mon, &mi)) {
                    RECT work = mi.rcWork;
                    adjustForAutoHideTaskbar(mon, work);
                    params->rgrc[0] = work;
                }
            }
        }
        if (result) *result = 0;
        return true;
    }
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
        if (!mmi) break;
        HMONITOR mon = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (mon && GetMonitorInfoW(mon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        if (window_) {
            const double scale = static_cast<double>(windowDpi(msg->hwnd)) / 96.0;
            if (window_->minimumWidth() > 0) {
                mmi->ptMinTrackSize.x = static_cast<LONG>(std::lround(window_->minimumWidth() * scale));
            }
            if (window_->minimumHeight() > 0) {
                mmi->ptMinTrackSize.y = static_cast<LONG>(std::lround(window_->minimumHeight() * scale));
            }
        }
        if (result) *result = 0;
        return true;
    }
    case WM_SHOWWINDOW: {
        if (msg->wParam == TRUE && msg->lParam == 0) {
            const LONG_PTR curStyle = GetWindowLongPtrW(msg->hwnd, GWL_STYLE);
            if ((curStyle & (WS_POPUP | WS_SYSMENU)) != 0
                || (curStyle & WS_CAPTION) == 0
                || (curStyle & WS_THICKFRAME) == 0) {
                applyWindowStyle();
            }
        }
        break;
    }
    case WM_NCLBUTTONDBLCLK: {
        if (msg->wParam == HTCAPTION) {
            ShowWindow(msg->hwnd, IsZoomed(msg->hwnd) ? SW_RESTORE : SW_MAXIMIZE);
            if (result) *result = 0;
            return true;
        }
        break;
    }
    case WM_NCHITTEST: {
        RECT winRect{};
        GetWindowRect(msg->hwnd, &winRect);
        const int x = GET_X_LPARAM(msg->lParam);
        const int y = GET_Y_LPARAM(msg->lParam);
        const double scale = static_cast<double>(windowDpi(msg->hwnd)) / 96.0;
        const int borderX = static_cast<int>(std::lround(6.0 * scale));
        const int borderTop = static_cast<int>(std::lround(5.0 * scale));
        const int borderBottom = static_cast<int>(std::lround(6.0 * scale));

        if (!IsZoomed(msg->hwnd)) {
            const bool left = x < winRect.left + borderX;
            const bool right = x >= winRect.right - borderX;
            const bool top = y < winRect.top + borderTop;
            const bool bottom = y >= winRect.bottom - borderBottom;

            if (top && left) { if (result) *result = HTTOPLEFT; return true; }
            if (top && right) { if (result) *result = HTTOPRIGHT; return true; }
            if (bottom && left) { if (result) *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { if (result) *result = HTBOTTOMRIGHT; return true; }
            if (left) { if (result) *result = HTLEFT; return true; }
            if (right) { if (result) *result = HTRIGHT; return true; }
            if (top) { if (result) *result = HTTOP; return true; }
            if (bottom) { if (result) *result = HTBOTTOM; return true; }
        }

        // Map empty TitleBar regions directly to native HTCAPTION so Windows handles
        // dragging, Aero Snap, drag-down-from-maximized, and double-click maximize
        // with zero QML pointer-grab latency or capture loss.
        if (titleBarDragEnabled_) {
            POINT clientPt{x, y};
            ScreenToClient(msg->hwnd, &clientPt);
            const double lx = static_cast<double>(clientPt.x) / scale;
            const double ly = static_cast<double>(clientPt.y) / scale;
            if (ly >= 0.0 && ly < static_cast<double>(titleBarHeightPx_)) {
                bool onInteractive = false;
                for (const QRectF& r : interactiveRects_) {
                    if (r.contains(lx, ly)) {
                        onInteractive = true;
                        break;
                    }
                }
                if (!onInteractive) {
                    if (result) *result = HTCAPTION;
                    return true;
                }
            }
        }

        // Explicitly return HTCLIENT so DefWindowProc never synthesizes native caption
        // or min/max/close hit-test zones over interactive controls.
        if (result) *result = HTCLIENT;
        return true;
    }
    default:
        break;
    }
    return false;
}

} // namespace sonora

#endif // _WIN32
