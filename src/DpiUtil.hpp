#pragma once
#include <windows.h>

namespace dpi {

// Returns the DPI of the monitor that contains the given window, or 96 as
// fallback. Safe to call before any window has been created (uses
// GetDpiForSystem in that case).
unsigned int GetForWindow(HWND hwnd);

// Convert device-independent pixels to physical pixels for a given DPI.
inline int Scale(int dip, unsigned int currentDpi) {
    return (dip * static_cast<int>(currentDpi) + 48) / 96;
}

} // namespace dpi
