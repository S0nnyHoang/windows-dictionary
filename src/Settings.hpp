#pragma once
#include <filesystem>
#include <string>

enum class SelectionMode {
    UiaOnly = 0,
    UiaPlusClipboardFallback = 1,
};

struct Settings {
    bool enabled = true;
    SelectionMode mode = SelectionMode::UiaOnly;
    bool runAtStartup = false;
    bool ttsEnabled = true;
    int  bubbleIdleMs = 4000;
    int  popupFontSize = 14;
};

class SettingsStore {
public:
    SettingsStore();

    const Settings& Get() const { return s_; }
    Settings& Mutable() { return s_; }

    void Load();
    void Save() const;

    const std::filesystem::path& Path() const { return path_; }

private:
    Settings s_;
    std::filesystem::path path_;
};
