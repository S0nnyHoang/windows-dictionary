#pragma once
#include <windows.h>

class TrayIcon {
public:
    // The tray icon callback (mouse events) is delivered as 'callbackMessage'
    // to 'ownerWindow'. Menu commands are delivered as WM_COMMAND.
    bool Install(HWND ownerWindow, UINT callbackMessage);
    void Uninstall();

    void SetEnabledChecked(bool enabled);
    void ShowContextMenu();

    // Re-add the icon when explorer.exe restarts. Owner should listen for the
    // registered "TaskbarCreated" message and call this.
    void Reinstall();

private:
    NOTIFYICONDATAW data_{};
    bool installed_ = false;
    bool enabled_ = true;
    HWND owner_ = nullptr;
};
