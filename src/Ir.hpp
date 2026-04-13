#pragma once
// Compact in-house "rich text" intermediate representation produced by the
// build-time StarDict converter and consumed by the renderer.
//
// Wire format (UTF-8 bytes stored in SQLite BLOB column):
//   RUN := 0x01 STYLE_BYTE TEXT
// where STYLE_BYTE bit flags follow IrStyleBits below. The text payload runs
// up to (but not including) the next 0x01 separator or end-of-buffer.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ir {

enum IrStyleBits : uint8_t {
    StyleBold       = 1 << 0,
    StyleItalic     = 1 << 1,
    StylePosTag     = 1 << 2, // part-of-speech label, e.g. "n.", "v."
    StyleExample    = 1 << 3, // example sentence
    StyleViTrans    = 1 << 4, // Vietnamese translation
    StyleNewlineBefore = 1 << 5,
};

struct TextRun {
    uint8_t style = 0;
    std::string text; // UTF-8
};

// Decode raw bytes into a sequence of runs. Returns empty vector on malformed input.
inline std::vector<TextRun> Decode(std::string_view bytes) {
    std::vector<TextRun> runs;
    size_t i = 0;
    // Tolerate a leading plain-text run with no separator.
    if (!bytes.empty() && static_cast<uint8_t>(bytes[0]) != 0x01) {
        size_t end = bytes.find('\x01');
        TextRun r;
        r.style = 0;
        r.text.assign(bytes.substr(0, end == std::string_view::npos ? bytes.size() : end));
        runs.push_back(std::move(r));
        if (end == std::string_view::npos) return runs;
        i = end;
    }
    while (i < bytes.size()) {
        if (static_cast<uint8_t>(bytes[i]) != 0x01) return {}; // malformed
        if (i + 1 >= bytes.size()) return {};
        TextRun r;
        r.style = static_cast<uint8_t>(bytes[i + 1]);
        size_t start = i + 2;
        size_t end = bytes.find('\x01', start);
        if (end == std::string_view::npos) end = bytes.size();
        r.text.assign(bytes.substr(start, end - start));
        runs.push_back(std::move(r));
        i = end;
    }
    return runs;
}

// Encode runs back into the wire format (used only by tests / converter).
inline std::string Encode(const std::vector<TextRun>& runs) {
    std::string out;
    for (const auto& r : runs) {
        out.push_back('\x01');
        out.push_back(static_cast<char>(r.style));
        out.append(r.text);
    }
    return out;
}

} // namespace ir
