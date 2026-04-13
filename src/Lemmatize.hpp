#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace lemmatize {

// Build a candidate list (cleaned word first, then progressively more
// aggressive lemmas). Caller queries each candidate against the dictionary in
// order; first hit wins. Pure ASCII rules — sufficient for English.
std::vector<std::wstring> Candidates(std::wstring_view word);

} // namespace lemmatize
