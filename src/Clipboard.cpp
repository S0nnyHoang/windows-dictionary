#include "Clipboard.hpp"

namespace clip {

std::optional<Snapshot> Capture(HWND owner) {
    Snapshot s;
    s.sequence = ::GetClipboardSequenceNumber();
    if (!::OpenClipboard(owner)) return std::nullopt;
    HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
    if (h) {
        auto* p = static_cast<const wchar_t*>(::GlobalLock(h));
        if (p) {
            s.text = p;
            s.hadText = true;
            ::GlobalUnlock(h);
        }
    }
    ::CloseClipboard();
    return s;
}

bool Restore(HWND owner, const Snapshot& snap) {
    if (!::OpenClipboard(owner)) return false;
    ::EmptyClipboard();
    if (snap.hadText) {
        size_t bytes = (snap.text.size() + 1) * sizeof(wchar_t);
        HGLOBAL g = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (g) {
            auto* dst = static_cast<wchar_t*>(::GlobalLock(g));
            if (dst) {
                memcpy(dst, snap.text.c_str(), bytes);
                ::GlobalUnlock(g);
                ::SetClipboardData(CF_UNICODETEXT, g);
            }
        }
    }
    ::CloseClipboard();
    return true;
}

std::optional<std::wstring> ReadText(HWND owner) {
    if (!::OpenClipboard(owner)) return std::nullopt;
    std::optional<std::wstring> result;
    HANDLE h = ::GetClipboardData(CF_UNICODETEXT);
    if (h) {
        auto* p = static_cast<const wchar_t*>(::GlobalLock(h));
        if (p) {
            result = std::wstring(p);
            ::GlobalUnlock(h);
        }
    }
    ::CloseClipboard();
    return result;
}

void SendCopyKeystroke() {
    INPUT in[4] = {};
    // Ctrl down
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = VK_CONTROL;
    // C down
    in[1].type = INPUT_KEYBOARD;
    in[1].ki.wVk = 'C';
    // C up
    in[2].type = INPUT_KEYBOARD;
    in[2].ki.wVk = 'C';
    in[2].ki.dwFlags = KEYEVENTF_KEYUP;
    // Ctrl up
    in[3].type = INPUT_KEYBOARD;
    in[3].ki.wVk = VK_CONTROL;
    in[3].ki.dwFlags = KEYEVENTF_KEYUP;
    ::SendInput(4, in, sizeof(INPUT));
}

} // namespace clip
