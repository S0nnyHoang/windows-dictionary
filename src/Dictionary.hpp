#pragma once

#include "Ir.hpp"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;
struct sqlite3_stmt;

struct LookupResult {
    std::wstring headword;          // matched dictionary head word
    std::wstring sourceTerm;        // what the user actually selected (post-clean)
    std::vector<ir::TextRun> runs;  // body
    std::vector<std::wstring> suggestions; // FTS prefix suggestions when no exact match
    bool found = false;
};

class Dictionary {
public:
    Dictionary() = default;
    ~Dictionary();

    Dictionary(const Dictionary&) = delete;
    Dictionary& operator=(const Dictionary&) = delete;

    // Lazy-opens the database on first use. Safe to call from a single worker
    // thread (the SQLite handle is not shared with other threads).
    LookupResult Lookup(std::wstring_view rawSelection);

    // Override database path. If unset, the executable directory is searched.
    void SetDatabasePath(std::filesystem::path p) { dbPath_ = std::move(p); }

private:
    bool EnsureOpen();
    bool TryExact(const std::wstring& word, LookupResult& out);
    void FillSuggestions(const std::wstring& word, LookupResult& out);

    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmtExact_ = nullptr;
    sqlite3_stmt* stmtFts_ = nullptr;
    std::filesystem::path dbPath_;
    bool openFailed_ = false;
};
