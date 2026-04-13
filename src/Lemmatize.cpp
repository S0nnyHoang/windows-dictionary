#include "Lemmatize.hpp"

#include <algorithm>

namespace lemmatize {

namespace {
bool EndsWith(std::wstring_view s, std::wstring_view suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

bool IsConsonant(wchar_t c) {
    switch (c) {
        case L'a': case L'e': case L'i': case L'o': case L'u': case L'y':
            return false;
        default:
            return c >= L'a' && c <= L'z';
    }
}

void PushUnique(std::vector<std::wstring>& out, std::wstring s) {
    if (s.empty()) return;
    if (std::find(out.begin(), out.end(), s) != out.end()) return;
    out.push_back(std::move(s));
}
} // namespace

std::vector<std::wstring> Candidates(std::wstring_view word) {
    std::vector<std::wstring> out;
    if (word.empty()) return out;

    std::wstring base(word);
    PushUnique(out, base);

    // Possessive
    if (EndsWith(base, L"'s") || EndsWith(base, L"\u2019s")) {
        PushUnique(out, base.substr(0, base.size() - 2));
    }

    // -ies → -y    (flies → fly)
    if (EndsWith(base, L"ies") && base.size() > 3) {
        PushUnique(out, base.substr(0, base.size() - 3) + L"y");
    }
    // -ied → -y    (tried → try)
    if (EndsWith(base, L"ied") && base.size() > 3) {
        PushUnique(out, base.substr(0, base.size() - 3) + L"y");
    }
    // -es          (boxes → box)
    if (EndsWith(base, L"es") && base.size() > 2) {
        PushUnique(out, base.substr(0, base.size() - 2));
    }
    // -s           (dogs → dog)
    if (EndsWith(base, L"s") && base.size() > 1 && !EndsWith(base, L"ss")) {
        PushUnique(out, base.substr(0, base.size() - 1));
    }
    // -ed
    if (EndsWith(base, L"ed") && base.size() > 2) {
        PushUnique(out, base.substr(0, base.size() - 2));
        PushUnique(out, base.substr(0, base.size() - 1)); // -d (loved → love)
    }
    // -ing
    if (EndsWith(base, L"ing") && base.size() > 3) {
        std::wstring stem = base.substr(0, base.size() - 3);
        PushUnique(out, stem);
        // Consonant doubling: running → run
        if (stem.size() >= 2 && stem.back() == stem[stem.size() - 2] && IsConsonant(stem.back())) {
            PushUnique(out, stem.substr(0, stem.size() - 1));
        }
        // -e dropped before -ing: making → make
        PushUnique(out, stem + L"e");
    }
    // -er / -est (comparatives)
    if (EndsWith(base, L"est") && base.size() > 3) {
        PushUnique(out, base.substr(0, base.size() - 3));
        PushUnique(out, base.substr(0, base.size() - 2)); // -e dropped (largest → large)
    }
    if (EndsWith(base, L"er") && base.size() > 2) {
        PushUnique(out, base.substr(0, base.size() - 2));
        PushUnique(out, base.substr(0, base.size() - 1));
    }
    // Hyphenated head word
    if (auto h = base.find(L'-'); h != std::wstring::npos && h > 0) {
        PushUnique(out, base.substr(0, h));
    }
    return out;
}

} // namespace lemmatize
