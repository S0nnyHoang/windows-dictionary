#include "Startup.hpp"

#include <windows.h>

namespace startup {

namespace {
constexpr const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr const wchar_t* kValue  = L"EnViDict";

std::wstring CurrentExePath() {
    wchar_t buf[MAX_PATH];
    DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0) return {};
    return std::wstring(buf, n);
}
} // namespace

bool IsEnabled() {
    HKEY h{};
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &h) != ERROR_SUCCESS) {
        return false;
    }
    DWORD type = 0, size = 0;
    LONG r = ::RegQueryValueExW(h, kValue, nullptr, &type, nullptr, &size);
    ::RegCloseKey(h);
    return r == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ);
}

bool Enable() {
    HKEY h{};
    if (::RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                          KEY_SET_VALUE, nullptr, &h, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    auto exe = CurrentExePath();
    if (exe.empty()) { ::RegCloseKey(h); return false; }
    std::wstring cmd = L"\"" + exe + L"\" --hidden";
    LONG r = ::RegSetValueExW(h, kValue, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(cmd.c_str()),
        static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
    ::RegCloseKey(h);
    return r == ERROR_SUCCESS;
}

bool Disable() {
    HKEY h{};
    if (::RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &h) != ERROR_SUCCESS) {
        return false;
    }
    LONG r = ::RegDeleteValueW(h, kValue);
    ::RegCloseKey(h);
    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

} // namespace startup
