#ifdef _WIN32

#include "platform/win32/Win32TrayIcon.h"
#include "platform/win32/SonoraResourceIds.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QWindow>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <string>

namespace sonora {

namespace {
constexpr UINT kTrayCallbackMsg = WM_USER + 201;
constexpr UINT kTrayIconId = 1;

constexpr UINT kCmdShow = 1001;
constexpr UINT kCmdPlayPause = 1002;
constexpr UINT kCmdPrev = 1003;
constexpr UINT kCmdNext = 1004;
constexpr UINT kCmdMini = 1005;
constexpr UINT kCmdQuit = 1006;
} // namespace

Win32TrayIcon::Win32TrayIcon(QObject* parent) : QObject(parent) {}

Win32TrayIcon::~Win32TrayIcon() {
    shutdown();
}

void Win32TrayIcon::attachWindow(QWindow* window) {
    if (!window) return;
    hwnd_ = static_cast<uintptr_t>(window->winId());
    if (!filterInstalled_ && QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
        filterInstalled_ = true;
    }
}

bool Win32TrayIcon::addTrayIcon() {
    if (!hwnd_) return false;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = reinterpret_cast<HWND>(hwnd_);
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = kTrayCallbackMsg;
    const HMODULE module = GetModuleHandleW(nullptr);
    nid.hIcon = static_cast<HICON>(LoadImageW(module, MAKEINTRESOURCEW(IDI_SONORA_APP), IMAGE_ICON,
                                             GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                             LR_SHARED));
    if (!nid.hIcon) {
        nid.hIcon = LoadIconW(module, MAKEINTRESOURCEW(IDI_SONORA_APP));
    }
    if (!nid.hIcon) {
        nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    const std::wstring tipW = tooltip_.toStdWString();
    const size_t cap = sizeof(nid.szTip) / sizeof(nid.szTip[0]);
    const size_t copyLen = std::min(tipW.size(), cap - 1);
    std::wmemcpy(nid.szTip, tipW.c_str(), copyLen);
    nid.szTip[copyLen] = L'\0';

    return Shell_NotifyIconW(NIM_ADD, &nid) != FALSE;
}

void Win32TrayIcon::init(const QString& tooltip) {
    tooltip_ = tooltip.left(120);
    if (!hwnd_ || active_) return;

    if (taskbarCreatedMsg_ == 0) {
        taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");
    }

    if (addTrayIcon()) {
        active_ = true;
    }
}

void Win32TrayIcon::updateTooltip(const QString& tooltip) {
    tooltip_ = tooltip.left(120);
    if (!active_ || !hwnd_) return;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = reinterpret_cast<HWND>(hwnd_);
    nid.uID = kTrayIconId;
    nid.uFlags = NIF_TIP;
    const std::wstring tipW = tooltip_.toStdWString();
    const size_t cap = sizeof(nid.szTip) / sizeof(nid.szTip[0]);
    const size_t copyLen = std::min(tipW.size(), cap - 1);
    std::wmemcpy(nid.szTip, tipW.c_str(), copyLen);
    nid.szTip[copyLen] = L'\0';
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void Win32TrayIcon::setPlayingState(bool playing) {
    playing_ = playing;
}

void Win32TrayIcon::shutdown() {
    if (active_ && hwnd_) {
        NOTIFYICONDATAW nid{};
        nid.cbSize = sizeof(NOTIFYICONDATAW);
        nid.hWnd = reinterpret_cast<HWND>(hwnd_);
        nid.uID = kTrayIconId;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        active_ = false;
    }
    if (filterInstalled_ && QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
        filterInstalled_ = false;
    }
}

void Win32TrayIcon::showContextMenu() {
    if (!hwnd_) {
        menuOpen_ = false;
        return;
    }
    HWND hwnd = reinterpret_cast<HWND>(hwnd_);
    HMENU menu = CreatePopupMenu();
    if (!menu) {
        menuOpen_ = false;
        return;
    }

    AppendMenuW(menu, MF_STRING, kCmdShow, L"显示 Sonora / Show Sonora");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdPlayPause, playing_ ? L"暂停 / Pause" : L"播放 / Play");
    AppendMenuW(menu, MF_STRING, kCmdPrev, L"上一首 / Previous");
    AppendMenuW(menu, MF_STRING, kCmdNext, L"下一首 / Next");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdMini, L"迷你模式 / Mini Mode");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdQuit, L"退出 / Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    const UINT cmd = TrackPopupMenuEx(
        menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        pt.x, pt.y, hwnd, nullptr);
    // Required by Microsoft KB135788 so subsequent right-clicks and outside clicks behave cleanly.
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
    menuOpen_ = false;

    switch (cmd) {
    case kCmdShow:      emit showWindowRequested(); break;
    case kCmdPlayPause: emit togglePlayPauseRequested(); break;
    case kCmdPrev:      emit previousRequested(); break;
    case kCmdNext:      emit nextRequested(); break;
    case kCmdMini:      emit toggleMiniModeRequested(); break;
    case kCmdQuit:      emit quitRequested(); break;
    default: break;
    }
}

bool Win32TrayIcon::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/) {
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
        return false;
    }
    MSG* msg = static_cast<MSG*>(message);
    if (!msg || reinterpret_cast<uintptr_t>(msg->hwnd) != hwnd_) {
        return false;
    }
    if (taskbarCreatedMsg_ != 0 && msg->message == taskbarCreatedMsg_) {
        if (active_) {
            addTrayIcon();
        }
        return false;
    }
    if (msg->message == kTrayCallbackMsg) {
        const LPARAM ev = LOWORD(msg->lParam);
        if (ev == WM_LBUTTONUP || ev == WM_LBUTTONDBLCLK) {
            emit showWindowRequested();
            return true;
        }
        if (ev == WM_RBUTTONUP || ev == WM_CONTEXTMENU) {
            if (!menuOpen_) {
                menuOpen_ = true;
                QMetaObject::invokeMethod(this, &Win32TrayIcon::showContextMenu, Qt::QueuedConnection);
            }
            return true;
        }
    }
    return false;
}

} // namespace sonora

#endif // _WIN32
