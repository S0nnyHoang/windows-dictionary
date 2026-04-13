#include "App.hpp"

#include "Normalize.hpp"
#include "Startup.hpp"
#include "SettingsWindow.hpp"

#include "../resources/resource.h"

#include <windowsx.h>

namespace {
constexpr const wchar_t* kMsgClass = L"EnViDictMessageWindow";
constexpr UINT_PTR kClipboardTimer = 2;
}

LRESULT CALLBACK App::MsgWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    App* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<App*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->msgWnd_ = hwnd;
    } else {
        self = reinterpret_cast<App*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return ::DefWindowProcW(hwnd, msg, wp, lp);

    if (msg == self->taskbarCreatedMsg_) {
        self->tray_.Reinstall();
        return 0;
    }

    switch (msg) {
    case WM_APP_TRAY_ICON:
        self->HandleTrayIcon(wp, lp);
        return 0;
    case WM_COMMAND:
        self->HandleTrayCommand(LOWORD(wp));
        return 0;
    case WM_APP_SELECTION_CHANGED: {
        auto* payload = reinterpret_cast<std::pair<std::wstring, RECT>*>(lp);
        self->HandleSelectionChanged(std::unique_ptr<std::pair<std::wstring, RECT>>(payload));
        return 0;
    }
    case WM_APP_LOOKUP_RESULT: {
        auto* r = reinterpret_cast<ResultBundle*>(lp);
        self->HandleLookupResult(std::unique_ptr<ResultBundle>(r));
        return 0;
    }
    case WM_APP_SHOW_SETTINGS:
        settings_window::Show(self->hInst_, self->msgWnd_, self->settings_);
        return 0;
    case WM_APP_TRY_CLIPBOARD_COPY: {
        POINT pt;
        pt.x = GET_X_LPARAM(lp);
        pt.y = GET_Y_LPARAM(lp);
        self->HandleClipboardProbe(pt);
        return 0;
    }
    case WM_TIMER:
        if (wp == kClipboardTimer) {
            ::KillTimer(hwnd, kClipboardTimer);
            self->HandleTakeClipboard();
            return 0;
        }
        break;
    }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

bool App::Initialize(HINSTANCE hInst) {
    hInst_ = hInst;

    ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    settings_.Load();

    // Hidden message-only window
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = App::MsgWndProc;
    wc.hInstance = hInst_;
    wc.lpszClassName = kMsgClass;
    ::RegisterClassExW(&wc);
    msgWnd_ = ::CreateWindowExW(0, kMsgClass, L"", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, nullptr, hInst_, this);
    if (!msgWnd_) return false;

    taskbarCreatedMsg_ = ::RegisterWindowMessageW(L"TaskbarCreated");

    if (!tray_.Install(msgWnd_, WM_APP_TRAY_ICON)) return false;
    tray_.SetEnabledChecked(settings_.Get().enabled);

    if (!bubble_.Create(hInst_, this)) return false;
    if (!popup_.Create(hInst_, this, &renderer_)) return false;

    tts_.Initialize();

    worker_ = std::thread(&App::WorkerMain, this);

    if (settings_.Get().enabled) {
        watcher_.Start(this,
            settings_.Get().mode == SelectionMode::UiaPlusClipboardFallback);
    }

    // Trim working set after init to reclaim transient allocations.
    ::SetProcessWorkingSetSize(::GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    return true;
}

int App::Run() {
    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}

void App::Shutdown() {
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        queue_.push_back({{}, {0, 0}, true});
    }
    queueCv_.notify_all();
    if (worker_.joinable()) worker_.join();

    watcher_.Stop();
    tts_.Shutdown();
    popup_.Destroy();
    bubble_.Destroy();
    tray_.Uninstall();

    if (msgWnd_) { ::DestroyWindow(msgWnd_); msgWnd_ = nullptr; }
    ::CoUninitialize();
}

void App::PostSelection(const std::wstring& text, RECT anchor) {
    auto* payload = new std::pair<std::wstring, RECT>(text, anchor);
    lastUiaSelectionTick_.store(::GetTickCount64(), std::memory_order_relaxed);
    ::PostMessageW(msgWnd_, WM_APP_SELECTION_CHANGED, 0,
                   reinterpret_cast<LPARAM>(payload));
}

void App::PostClipboardProbe(POINT screen) {
    LPARAM lp = MAKELPARAM(screen.x, screen.y);
    ::PostMessageW(msgWnd_, WM_APP_TRY_CLIPBOARD_COPY, 0, lp);
}

void App::HandleTrayIcon(WPARAM /*wp*/, LPARAM lp) {
    UINT event = LOWORD(lp);
    if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
        tray_.ShowContextMenu();
    } else if (event == WM_LBUTTONDBLCLK) {
        settings_window::Show(hInst_, msgWnd_, settings_);
    }
}

void App::HandleTrayCommand(WORD id) {
    switch (id) {
    case IDM_TRAY_ENABLE: {
        bool newState = !settings_.Get().enabled;
        settings_.Mutable().enabled = newState;
        settings_.Save();
        tray_.SetEnabledChecked(newState);
        if (newState) {
            watcher_.Start(this,
                settings_.Get().mode == SelectionMode::UiaPlusClipboardFallback);
        } else {
            watcher_.Stop();
            bubble_.Hide();
            popup_.Hide();
        }
        break;
    }
    case IDM_TRAY_SETTINGS:
        settings_window::Show(hInst_, msgWnd_, settings_);
        // Re-apply startup + fallback state after user edits.
        if (settings_.Get().runAtStartup) startup::Enable(); else startup::Disable();
        if (settings_.Get().enabled) {
            watcher_.Stop();
            watcher_.Start(this,
                settings_.Get().mode == SelectionMode::UiaPlusClipboardFallback);
        }
        break;
    case IDM_TRAY_ABOUT:
        ::MessageBoxW(nullptr,
            L"EnViDict — lightweight English-Vietnamese popup dictionary.\n"
            L"Select any word and click the floating D bubble.\n\n"
            L"Dictionary data: Open Vietnamese Dictionary Project.",
            L"About EnViDict", MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_TRAY_QUIT:
        ::PostQuitMessage(0);
        break;
    }
}

void App::HandleSelectionChanged(std::unique_ptr<std::pair<std::wstring, RECT>> payload) {
    if (!payload) return;
    auto cleaned = normalize::CleanForLookup(payload->first);
    if (cleaned.empty()) return;
    RECT anchor = payload->second;
    lastAnchor_.x = anchor.right;
    lastAnchor_.y = anchor.bottom;
    bubble_.ShowAt(anchor, payload->first);
}

void App::OnBubbleClicked(const std::wstring& text, POINT anchor) {
    lastAnchor_ = anchor;
    EnqueueLookup(text, anchor);
}

void App::EnqueueLookup(std::wstring text, POINT anchor) {
    {
        std::lock_guard<std::mutex> lk(queueMutex_);
        queue_.push_back({std::move(text), anchor, false});
    }
    queueCv_.notify_one();
}

void App::HandleLookupResult(std::unique_ptr<ResultBundle> bundle) {
    if (!bundle) return;
    auto owned = std::make_unique<LookupResult>(std::move(bundle->result));
    popup_.ShowAt(bundle->anchor, std::move(owned));
}

void App::HandleClipboardProbe(POINT screen) {
    if (settings_.Get().mode != SelectionMode::UiaPlusClipboardFallback) return;

    // If UIA already delivered a selection in the last 200 ms, skip.
    uint64_t now = ::GetTickCount64();
    uint64_t last = lastUiaSelectionTick_.load(std::memory_order_relaxed);
    if (last != 0 && now - last < 200) return;

    auto snap = clip::Capture(msgWnd_);
    if (!snap) return;
    probe_.before = *snap;
    probe_.active = true;
    probe_.anchor = screen;

    clip::SendCopyKeystroke();
    ::SetTimer(msgWnd_, kClipboardTimer, 60, nullptr);
}

void App::HandleTakeClipboard() {
    if (!probe_.active) return;
    probe_.active = false;

    auto now = clip::ReadText(msgWnd_);
    // Best-effort restore so we don't leave the user's clipboard holding the
    // fragment we just looked up.
    clip::Restore(msgWnd_, probe_.before);

    if (!now) return;
    auto cleaned = normalize::CleanForLookup(*now);
    if (cleaned.empty()) return;
    if (probe_.before.hadText && *now == probe_.before.text) return;

    // Show bubble near the mouse-up position; anchor is a zero-size rect.
    RECT anchor{ probe_.anchor.x, probe_.anchor.y, probe_.anchor.x, probe_.anchor.y };
    lastAnchor_ = probe_.anchor;
    bubble_.ShowAt(anchor, *now);
}

void App::Speak(const std::wstring& text) {
    if (settings_.Get().ttsEnabled) tts_.Speak(text);
}

void App::WorkerMain() {
    for (;;) {
        Request req;
        {
            std::unique_lock<std::mutex> lk(queueMutex_);
            queueCv_.wait(lk, [this] { return !queue_.empty(); });
            req = std::move(queue_.front());
            queue_.pop_front();
        }
        if (req.stop) break;

        auto* bundle = new ResultBundle{ dict_.Lookup(req.text), req.anchor };
        ::PostMessageW(msgWnd_, WM_APP_LOOKUP_RESULT, 0,
                       reinterpret_cast<LPARAM>(bundle));
    }
}
