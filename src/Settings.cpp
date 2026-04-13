#include "Settings.hpp"

#include <windows.h>
#include <shlobj.h>
#include <knownfolders.h>

#include <string>

namespace {
constexpr const wchar_t* kSection = L"EnViDict";

std::filesystem::path AppDataDir() {
    PWSTR raw = nullptr;
    if (FAILED(::SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
        return {};
    }
    std::filesystem::path p(raw);
    ::CoTaskMemFree(raw);
    p /= L"EnViDict";
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    return p;
}

int ReadInt(const wchar_t* key, int def, const std::wstring& path) {
    return static_cast<int>(::GetPrivateProfileIntW(kSection, key, def, path.c_str()));
}

void WriteInt(const wchar_t* key, int value, const std::wstring& path) {
    wchar_t buf[32];
    swprintf_s(buf, L"%d", value);
    ::WritePrivateProfileStringW(kSection, key, buf, path.c_str());
}
} // namespace

SettingsStore::SettingsStore() {
    auto dir = AppDataDir();
    if (!dir.empty()) path_ = dir / L"settings.ini";
}

void SettingsStore::Load() {
    if (path_.empty()) return;
    const auto p = path_.wstring();
    s_.enabled       = ReadInt(L"enabled",       1, p) != 0;
    s_.mode          = static_cast<SelectionMode>(ReadInt(L"mode", 0, p));
    s_.runAtStartup  = ReadInt(L"runAtStartup",  0, p) != 0;
    s_.ttsEnabled    = ReadInt(L"ttsEnabled",    1, p) != 0;
    s_.bubbleIdleMs  = ReadInt(L"bubbleIdleMs",  4000, p);
    s_.popupFontSize = ReadInt(L"popupFontSize", 14, p);
}

void SettingsStore::Save() const {
    if (path_.empty()) return;
    const auto p = path_.wstring();
    WriteInt(L"enabled",       s_.enabled ? 1 : 0,         p);
    WriteInt(L"mode",          static_cast<int>(s_.mode),  p);
    WriteInt(L"runAtStartup",  s_.runAtStartup ? 1 : 0,    p);
    WriteInt(L"ttsEnabled",    s_.ttsEnabled ? 1 : 0,      p);
    WriteInt(L"bubbleIdleMs",  s_.bubbleIdleMs,            p);
    WriteInt(L"popupFontSize", s_.popupFontSize,           p);
}
