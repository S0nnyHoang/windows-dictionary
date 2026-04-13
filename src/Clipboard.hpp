#pragma once
#include <windows.h>
#include <optional>
#include <string>

namespace clip {

struct Snapshot {
    bool  hadText = false;
    std::wstring text;
    DWORD sequence = 0;
};

// Capture CF_UNICODETEXT (if any) and current clipboard sequence number.
// Returns std::nullopt if the clipboard could not be opened.
std::optional<Snapshot> Capture(HWND owner);

// Write the snapshot back to the clipboard. Best-effort; if another app
// writes between our copy and restore, we may trample their data, but in
// practice the clipboard fallback round-trip is ~60 ms and this is rare.
bool Restore(HWND owner, const Snapshot& snap);

// Read the current CF_UNICODETEXT, if any.
std::optional<std::wstring> ReadText(HWND owner);

// Synthesize Ctrl+C keystrokes via SendInput.
void SendCopyKeystroke();

} // namespace clip
