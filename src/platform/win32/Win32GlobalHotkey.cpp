#ifdef _WIN32

#include "platform/win32/Win32GlobalHotkey.h"

#include <QCoreApplication>
#include <QWindow>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace virela {

namespace {
constexpr int kHkPlayPause = 0x5101;
constexpr int kHkNext = 0x5102;
constexpr int kHkPrev = 0x5103;
constexpr int kHkVolUp = 0x5104;
constexpr int kHkVolDown = 0x5105;
constexpr int kHkMini = 0x5106;
} // namespace

Win32GlobalHotkey::Win32GlobalHotkey(QObject* parent) : QObject(parent) {}

Win32GlobalHotkey::~Win32GlobalHotkey() {
    unregisterAll();
    if (filterInstalled_ && QCoreApplication::instance()) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
        filterInstalled_ = false;
    }
}

void Win32GlobalHotkey::attachWindow(QWindow* window) {
    if (!window) return;
    unregisterAll();
    hwnd_ = static_cast<uintptr_t>(window->winId());
    if (!filterInstalled_ && QCoreApplication::instance()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
        filterInstalled_ = true;
    }
    if (enabled_) {
        registerAll();
    }
}

void Win32GlobalHotkey::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    if (enabled_) {
        registerAll();
    } else {
        unregisterAll();
    }
}

void Win32GlobalHotkey::registerAll() {
    if (!hwnd_ || registered_) return;
    HWND hwnd = reinterpret_cast<HWND>(hwnd_);
    const UINT mods = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;

    RegisterHotKey(hwnd, kHkPlayPause, mods, VK_SPACE);
    RegisterHotKey(hwnd, kHkNext, mods, VK_RIGHT);
    RegisterHotKey(hwnd, kHkPrev, mods, VK_LEFT);
    RegisterHotKey(hwnd, kHkVolUp, mods, VK_UP);
    RegisterHotKey(hwnd, kHkVolDown, mods, VK_DOWN);
    RegisterHotKey(hwnd, kHkMini, mods, 'M');
    registered_ = true;
}

void Win32GlobalHotkey::unregisterAll() {
    if (!hwnd_ || !registered_) return;
    HWND hwnd = reinterpret_cast<HWND>(hwnd_);
    UnregisterHotKey(hwnd, kHkPlayPause);
    UnregisterHotKey(hwnd, kHkNext);
    UnregisterHotKey(hwnd, kHkPrev);
    UnregisterHotKey(hwnd, kHkVolUp);
    UnregisterHotKey(hwnd, kHkVolDown);
    UnregisterHotKey(hwnd, kHkMini);
    registered_ = false;
}

bool Win32GlobalHotkey::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/) {
    if (eventType != "windows_generic_MSG" && eventType != "windows_dispatcher_MSG") {
        return false;
    }
    MSG* msg = static_cast<MSG*>(message);
    if (!msg || msg->message != WM_HOTKEY || reinterpret_cast<uintptr_t>(msg->hwnd) != hwnd_) {
        return false;
    }

    switch (static_cast<int>(msg->wParam)) {
    case kHkPlayPause: emit togglePlayPauseRequested(); return true;
    case kHkNext:      emit nextRequested(); return true;
    case kHkPrev:      emit previousRequested(); return true;
    case kHkVolUp:     emit volumeUpRequested(); return true;
    case kHkVolDown:   emit volumeDownRequested(); return true;
    case kHkMini:      emit toggleMiniModeRequested(); return true;
    default: break;
    }
    return false;
}

} // namespace virela

#endif // _WIN32
