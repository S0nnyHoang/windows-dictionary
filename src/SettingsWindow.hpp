#pragma once
#include "Settings.hpp"

#include <windows.h>

namespace settings_window {

// Modal dialog (standard Win32 DialogBox). Returns true if the user clicked OK
// and settings were saved.
bool Show(HINSTANCE hInst, HWND owner, SettingsStore& store);

} // namespace settings_window
