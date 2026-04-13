#include "Dictionary.hpp"

#include "Lemmatize.hpp"
#include "Normalize.hpp"

#include "../third_party/sqlite/sqlite3.h"

#include <windows.h>

namespace {

std::filesystem::path ExecutableDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0) return {};
    std::filesystem::path p(buf);
    return p.parent_path();
}

} // namespace

Dictionary::~Dictionary() {
    if (stmtExact_) sqlite3_finalize(stmtExact_);
    if (stmtFts_)   sqlite3_finalize(stmtFts_);
    if (db_)        sqlite3_close(db_);
}

bool Dictionary::EnsureOpen() {
    if (db_) return true;
    if (openFailed_) return false;

    if (dbPath_.empty()) {
        dbPath_ = ExecutableDir() / L"dictionary.db";
    }

    int rc = sqlite3_open_v2(
        normalize::ToUtf8(dbPath_.wstring()).c_str(),
        &db_,
        SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX,
        nullptr);
    if (rc != SQLITE_OK) {
        if (db_) { sqlite3_close(db_); db_ = nullptr; }
        openFailed_ = true;
        return false;
    }

    // Hint the OS to memory-map the file so private commit stays small.
    sqlite3_exec(db_, "PRAGMA mmap_size = 16777216;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA query_only = 1;",       nullptr, nullptr, nullptr);

    rc = sqlite3_prepare_v2(db_,
        "SELECT definition_ir FROM entries WHERE word = ?1 COLLATE NOCASE LIMIT 1;",
        -1, &stmtExact_, nullptr);
    if (rc != SQLITE_OK) { openFailed_ = true; return false; }

    // FTS table is optional; ignore failure (the converter may have skipped it).
    sqlite3_prepare_v2(db_,
        "SELECT word FROM entries_fts WHERE word MATCH ?1 || '*' LIMIT 5;",
        -1, &stmtFts_, nullptr);

    return true;
}

bool Dictionary::TryExact(const std::wstring& word, LookupResult& out) {
    if (!stmtExact_) return false;
    sqlite3_reset(stmtExact_);
    sqlite3_clear_bindings(stmtExact_);

    const std::string utf8 = normalize::ToUtf8(word);
    sqlite3_bind_text(stmtExact_, 1, utf8.c_str(), static_cast<int>(utf8.size()), SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmtExact_);
    if (rc != SQLITE_ROW) return false;

    const void* blob = sqlite3_column_blob(stmtExact_, 0);
    int blobSize = sqlite3_column_bytes(stmtExact_, 0);
    if (blob && blobSize > 0) {
        std::string_view bytes(static_cast<const char*>(blob), static_cast<size_t>(blobSize));
        out.runs = ir::Decode(bytes);
    }
    out.headword = word;
    out.found = true;
    return true;
}

void Dictionary::FillSuggestions(const std::wstring& word, LookupResult& out) {
    if (!stmtFts_) return;
    sqlite3_reset(stmtFts_);
    sqlite3_clear_bindings(stmtFts_);
    const std::string utf8 = normalize::ToUtf8(word);
    sqlite3_bind_text(stmtFts_, 1, utf8.c_str(), static_cast<int>(utf8.size()), SQLITE_TRANSIENT);

    while (sqlite3_step(stmtFts_) == SQLITE_ROW) {
        const unsigned char* w = sqlite3_column_text(stmtFts_, 0);
        if (w) {
            out.suggestions.push_back(
                normalize::FromUtf8(reinterpret_cast<const char*>(w)));
        }
    }
}

LookupResult Dictionary::Lookup(std::wstring_view rawSelection) {
    LookupResult result;
    result.sourceTerm = std::wstring(rawSelection);

    auto cleaned = normalize::CleanForLookup(rawSelection);
    if (cleaned.empty()) return result;

    if (!EnsureOpen()) return result;

    auto candidates = lemmatize::Candidates(cleaned);
    for (const auto& c : candidates) {
        if (TryExact(c, result)) return result;
    }

    FillSuggestions(cleaned, result);
    return result;
}
