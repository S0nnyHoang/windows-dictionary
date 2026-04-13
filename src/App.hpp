#pragma once

#include "BubbleWindow.hpp"
#include "Clipboard.hpp"
#include "Dictionary.hpp"
#include "PopupWindow.hpp"
#include "Renderer.hpp"
#include "SelectionWatcher.hpp"
#include "Settings.hpp"
#include "Tts.hpp"
#include "TrayIcon.hpp"

#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

// Private WM_APP messages marshalled to the message-only window.
enum AppMessages {
    WM_APP_TRAY_ICON          = WM_APP + 1,
    WM_APP_SELECTION_CHANGED  = WM_APP + 2,
    WM_APP_LOOKUP_RESULT      = WM_APP + 3,
    WM_APP_SHOW_SETTINGS      = WM_APP + 4,
    WM_APP_TRY_CLIPBOARD_COPY = WM_APP + 5,
    WM_APP_SHOW_POPUP         = WM_APP + 6,
    WM_APP_TAKE_CLIPBOARD     = WM_APP + 7,
};

class App {
public:
    bool Initialize(HINSTANCE hInst);
    int  Run();
    void Shutdown();

    // Called by modules on non-UI threads.
    void PostSelection(const std::wstring& text, RECT anchor);
    void PostClipboardProbe(POINT screen);

    // Called by UI-thread code.
    void OnBubbleClicked(const std::wstring& text, POINT anchor);
    void Speak(const std::wstring& text);

    bool TtsAvailable() const { return tts_.IsAvailable(); }
    int  BubbleIdleMs() const { return settings_.Get().bubbleIdleMs; }
    int  PopupFontSize() const { return settings_.Get().popupFontSize; }

    HWND MessageWindow() const { return msgWnd_; }

private:
    static LRESULT CALLBACK MsgWndProc(HWND, UINT, WPARAM, LPARAM);
    void HandleTrayIcon(WPARAM, LPARAM);
    void HandleTrayCommand(WORD id);
    void HandleSelectionChanged(std::unique_ptr<std::pair<std::wstring, RECT>> payload);
    struct ResultBundle {
        LookupResult result;
        POINT anchor;
    };
    void HandleLookupResult(std::unique_ptr<ResultBundle> bundle);
    void HandleClipboardProbe(POINT screen);
    void HandleTakeClipboard();

    // Worker thread
    void WorkerMain();
    void EnqueueLookup(std::wstring text, POINT anchor);

    HINSTANCE hInst_ = nullptr;
    HWND msgWnd_ = nullptr;
    UINT taskbarCreatedMsg_ = 0;

    TrayIcon tray_;
    BubbleWindow bubble_;
    PopupWindow popup_;
    Renderer renderer_;
    SettingsStore settings_;
    SelectionWatcher watcher_;
    Dictionary dict_;
    Tts tts_;

    // Worker
    std::thread worker_;
    std::mutex queueMutex_;
    std::condition_variable queueCv_;
    struct Request {
        std::wstring text;
        POINT anchor;
        bool stop = false;
    };
    std::deque<Request> queue_;

    // Clipboard fallback state
    struct ClipboardProbe {
        clip::Snapshot before;
        POINT anchor{0, 0};
        bool active = false;
    };
    ClipboardProbe probe_;

    // Most recent valid selection anchor (for bubble-click lookups)
    POINT lastAnchor_{0, 0};

    std::atomic<uint64_t> lastUiaSelectionTick_{0};
};
