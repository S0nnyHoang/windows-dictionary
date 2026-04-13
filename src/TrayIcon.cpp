#include "TrayIcon.hpp"

#include "../resources/resource.h"

#include <shellapi.h>

namespace {
constexpr UINT kTrayIconId = 1;
}

bool TrayIcon::Install(HWND ownerWindow, UINT callbackMessage) {
    owner_ = ownerWindow;
    data_ = {};
    data_.cbSize = sizeof(data_);
    data_.hWnd = ownerWindow;
    data_.uID = kTrayIconId;
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = callbackMessage;
    // Try the embedded app icon, fall back to the standard application icon.
    data_.hIcon = ::LoadIconW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));
    if (!data_.hIcon) data_.hIcon = ::LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(data_.szTip, L"EnViDict — English-Vietnamese popup dictionary");
    data_.uVersion = NOTIFYICON_VERSION_4;

    if (!::Shell_NotifyIconW(NIM_ADD, &data_)) return false;
    ::Shell_NotifyIconW(NIM_SETVERSION, &data_);
    installed_ = true;
    return true;
}

void TrayIcon::Reinstall() {
    if (installed_) return;
    Install(owner_, data_.uCallbackMessage);
}

void TrayIcon::Uninstall() {
    if (installed_) {
        ::Shell_NotifyIconW(NIM_DELETE, &data_);
        installed_ = false;
    }
}

void TrayIcon::SetEnabledChecked(bool enabled) {
    enabled_ = enabled;
}

void TrayIcon::ShowContextMenu() {
    HMENU menu = ::CreatePopupMenu();
    if (!menu) return;
    ::AppendMenuW(menu, MF_STRING | (enabled_ ? MF_CHECKED : MF_UNCHECKED),
                  IDM_TRAY_ENABLE,   L"Enabled");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, IDM_TRAY_SETTINGS, L"Settings…");
    ::AppendMenuW(menu, MF_STRING, IDM_TRAY_ABOUT,    L"About");
    ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    ::AppendMenuW(menu, MF_STRING, IDM_TRAY_QUIT,     L"Quit");

    POINT pt;
    ::GetCursorPos(&pt);
    // Required for tracker menu to dismiss properly when used from a tray callback.
    ::SetForegroundWindow(owner_);
    ::TrackPopupMenuEx(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                       pt.x, pt.y, owner_, nullptr);
    ::PostMessageW(owner_, WM_NULL, 0, 0);
    ::DestroyMenu(menu);
}
