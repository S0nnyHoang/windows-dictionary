#include "DpiUtil.hpp"

namespace dpi {

unsigned int GetForWindow(HWND hwnd) {
    if (hwnd) {
        const UINT d = ::GetDpiForWindow(hwnd);
        if (d != 0) return d;
    }
    return ::GetDpiForSystem();
}

} // namespace dpi
