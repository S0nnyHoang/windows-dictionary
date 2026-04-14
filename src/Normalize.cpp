#include "Normalize.hpp"

#include <windows.h>
#include <winnls.h>

#include <algorithm>
#include <vector>

namespace normalize {

namespace {
constexpr bool IsWS(wchar_t c) {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' ||
           c == 0x00A0 /* NBSP */ || c == 0x200B /* ZWSP */ ||
           c == 0x3000 /* ideographic space */;
}
} // namespace

std::wstring Trim(std::wstring_view s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && IsWS(s[b])) ++b;
    while (e > b && IsWS(s[e - 1])) --e;
    return std::wstring(s.substr(b, e - b));
}

std::wstring Lower(std::wstring s) {
    if (!s.empty()) {
        ::CharLowerBuffW(s.data(), static_cast<DWORD>(s.size()));
    }
    return s;
}

std::wstring StripPunct(std::wstring s) {
    if (s.empty()) return s;
    std::vector<WORD> types(s.size(), 0);
    if (!::GetStringTypeW(CT_CTYPE1, s.c_str(), static_cast<int>(s.size()), types.data())) {
        return s;
    }
    size_t b = 0, e = s.size();
    while (b < e && (types[b] & C1_PUNCT)) ++b;
    while (e > b && (types[e - 1] & C1_PUNCT)) --e;
    return s.substr(b, e - b);
}

bool HasLatinLetter(std::wstring_view s) {
    for (wchar_t c : s) {
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z')) return true;
    }
    return false;
}

bool HasNewline(std::wstring_view s) {
    return s.find_first_of(L"\r\n") != std::wstring_view::npos;
}

static std::wstring NfcNormalize(std::wstring s) {
    if (s.empty()) return s;
    int needed = ::NormalizeString(NormalizationC, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return s;
    std::wstring out(static_cast<size_t>(needed), L'\0');
    int written = ::NormalizeString(NormalizationC, s.c_str(), static_cast<int>(s.size()),
                                    out.data(), needed);
    if (written <= 0) return s;
    out.resize(static_cast<size_t>(written));
    return out;
}

std::wstring CleanForLookup(std::wstring_view raw, size_t maxLen) {
    auto s = Trim(raw);
    if (s.empty() || s.size() > maxLen) return {};
    if (HasNewline(s)) return {};
    if (!HasLatinLetter(s)) return {};
    s = NfcNormalize(std::move(s));
    s = Lower(std::move(s));
    s = StripPunct(std::move(s));
    return s;
}

std::string ToUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                  nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                          out.data(), n, nullptr, nullptr);
    return out;
}

std::wstring FromUtf8(std::string_view s) {
    if (s.empty()) return {};
    int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

} // namespace normalize
