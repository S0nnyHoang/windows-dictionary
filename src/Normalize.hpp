#pragma once
#include <string>
#include <string_view>

namespace normalize {

// Trim leading/trailing ASCII + Unicode whitespace (space, tab, NBSP, ZWSP).
std::wstring Trim(std::wstring_view s);

// Lowercase via CharLowerBuffW (locale-aware).
std::wstring Lower(std::wstring s);

// Strip leading/trailing punctuation using GetStringTypeW(C1_PUNCT).
std::wstring StripPunct(std::wstring s);

// Returns true if the string contains at least one Latin letter (a-z, A-Z).
bool HasLatinLetter(std::wstring_view s);

// Returns true if the string contains a CR/LF.
bool HasNewline(std::wstring_view s);

// One-shot helper applied to raw selection text. Performs trim → reject → NFC →
// lower → strip punctuation. Returns empty string when the input should be
// dropped (empty / too long / no letters / multi-line).
std::wstring CleanForLookup(std::wstring_view raw, size_t maxLen = 64);

// UTF-16 → UTF-8 helper (uses WideCharToMultiByte).
std::string ToUtf8(std::wstring_view w);
// UTF-8 → UTF-16.
std::wstring FromUtf8(std::string_view s);

} // namespace normalize
