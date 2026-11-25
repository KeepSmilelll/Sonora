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
    HWND hwnd = reinterpret_cast<HWND>(hwnd_);

    // Ensure standard Win32 styles for Aero Snap, shadow, and maximize/minimize animations
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    style |= WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetWindowLongPtrW(hwnd, GWL_STYLE, style);

    // Extend DWM glass frame over the entire client area so Mica/Acrylic & shadow work seamlessly
    const MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Request Windows 11 rounded corners when supported
    const DWORD cornerPref = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, kDwmwaWindowCornerPreference, &cornerPref, sizeof(cornerPref));

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);

    if (!installed_ && QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
        installed_ = true;
    }
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
            if (IsZoomed(msg->hwnd)) {
                // Compensate for invisible resize border when maximized so content never clips off-screen
                const int frameX = GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                const int frameY = GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                params->rgrc[0].left += frameX;
                params->rgrc[0].top += frameY;
                params->rgrc[0].right -= frameX;
                params->rgrc[0].bottom -= frameY;
            }
            *result = 0;
            return true;
        }
        break;
    }
    case WM_NCHITTEST: {
        RECT winRect{};
        GetWindowRect(msg->hwnd, &winRect);
        const int x = GET_X_LPARAM(msg->lParam);
        const int y = GET_Y_LPARAM(msg->lParam);
        const double dpr = window_ ? window_->devicePixelRatio() : 1.0;
        const int border = static_cast<int>(6 * dpr);

        if (!IsZoomed(msg->hwnd)) {
            const bool left = x < winRect.left + border;
            const bool right = x >= winRect.right - border;
            const bool top = y < winRect.top + border;
            const bool bottom = y >= winRect.bottom - border;

            if (top && left) { *result = HTTOPLEFT; return true; }
            if (top && right) { *result = HTTOPRIGHT; return true; }
            if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
            if (left) { *result = HTLEFT; return true; }
            if (right) { *result = HTRIGHT; return true; }
            if (top) { *result = HTTOP; return true; }
            if (bottom) { *result = HTBOTTOM; return true; }
        }

        // Caption drag zone: top titlebar excluding interactive center search & right caption buttons
        const int captionH = static_cast<int>(titleBarHeightPx_ * dpr);
        const int winWidth = winRect.right - winRect.left;
        const int rightButtonsW = static_cast<int>(210 * dpr);
        const int leftControlsW = static_cast<int>(220 * dpr);
        const int localX = x - winRect.left;
        const int localY = y - winRect.top;

        if (localY >= 0 && localY < captionH) {
            // Allow dragging on empty margins of the titlebar; QML DragHandler also supports inner areas
            if (localX > leftControlsW && localX < (winWidth - rightButtonsW)) {
                // Keep search input clickable in center
                return false;
            }
        }
        break;
    }
    default:
        break;
    }
    return false;
}

} // namespace sonora

#endif // _WIN32
