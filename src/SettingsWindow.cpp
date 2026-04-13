#include "SettingsWindow.hpp"

#include "Startup.hpp"

#include "../resources/resource.h"

#include <commctrl.h>
#include <windows.h>

#pragma comment(lib, "comctl32.lib")

namespace settings_window {

namespace {

struct DlgState {
    SettingsStore* store;
    HWND hEnabled;
    HWND hFallback;
    HWND hStartup;
    HWND hTts;
    HWND hIdleMs;
    HWND hFontSize;
    bool result = false;
};

LRESULT CALLBACK DlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* s = reinterpret_cast<DlgState*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }
    case WM_CLOSE:
        ::DestroyWindow(hwnd);
        return 0;
    case WM_COMMAND:
        if (!s) break;
        if (LOWORD(wp) == IDC_SETTINGS_OK) {
            auto& st = s->store->Mutable();
            st.enabled      = ::SendMessageW(s->hEnabled,  BM_GETCHECK, 0, 0) == BST_CHECKED;
            st.mode         = (::SendMessageW(s->hFallback, BM_GETCHECK, 0, 0) == BST_CHECKED)
                                ? SelectionMode::UiaPlusClipboardFallback
                                : SelectionMode::UiaOnly;
            st.runAtStartup = ::SendMessageW(s->hStartup,  BM_GETCHECK, 0, 0) == BST_CHECKED;
            st.ttsEnabled   = ::SendMessageW(s->hTts,      BM_GETCHECK, 0, 0) == BST_CHECKED;
            wchar_t buf[32];
            ::GetWindowTextW(s->hIdleMs, buf, 32);
            st.bubbleIdleMs = _wtoi(buf);
            if (st.bubbleIdleMs < 500) st.bubbleIdleMs = 500;
            ::GetWindowTextW(s->hFontSize, buf, 32);
            st.popupFontSize = _wtoi(buf);
            if (st.popupFontSize < 8) st.popupFontSize = 8;
            if (st.popupFontSize > 32) st.popupFontSize = 32;
            s->store->Save();
            if (st.runAtStartup) startup::Enable(); else startup::Disable();
            s->result = true;
            ::DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wp) == IDC_SETTINGS_CANCEL) {
            ::DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_DESTROY:
        // Wake the local modal loop so it re-checks IsWindow and exits.
        ::PostMessageW(nullptr, WM_NULL, 0, 0);
        return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

HWND MakeLabel(HWND parent, const wchar_t* text, int x, int y, int w) {
    return ::CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE,
                             x, y, w, 20, parent, nullptr, nullptr, nullptr);
}
HWND MakeCheck(HWND parent, const wchar_t* text, int x, int y, int w, int id, bool checked) {
    HWND h = ::CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        x, y, w, 24, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        nullptr, nullptr);
    ::SendMessageW(h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return h;
}
HWND MakeEdit(HWND parent, const wchar_t* text, int x, int y, int w, int id) {
    return ::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
        x, y, w, 22, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}
HWND MakeButton(HWND parent, const wchar_t* text, int x, int y, int w, int id, bool def) {
    return ::CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | (def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
        x, y, w, 26, parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

} // namespace

bool Show(HINSTANCE hInst, HWND owner, SettingsStore& store) {
    static bool registered = false;
    if (!registered) {
        INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES };
        ::InitCommonControlsEx(&icc);

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = DlgProc;
        wc.hInstance = hInst;
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"EnViDictSettings";
        ::RegisterClassExW(&wc);
        registered = true;
    }

    DlgState state{};
    state.store = &store;

    HWND hwnd = ::CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"EnViDictSettings", L"EnViDict — Settings",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 320,
        owner, nullptr, hInst, &state);
    if (!hwnd) return false;

    const auto& cur = store.Get();
    int x = 16, y = 16, w = 340;
    state.hEnabled  = MakeCheck(hwnd, L"Enable lookup bubble", x, y, w,
                                IDC_SETTINGS_ENABLED, cur.enabled); y += 28;
    state.hFallback = MakeCheck(hwnd, L"Use clipboard fallback (for Chrome, Electron, Notepad)",
                                x, y, w, IDC_SETTINGS_MODE,
                                cur.mode == SelectionMode::UiaPlusClipboardFallback); y += 28;
    state.hStartup  = MakeCheck(hwnd, L"Run EnViDict at Windows startup",
                                x, y, w, IDC_SETTINGS_RUNATSTARTUP, cur.runAtStartup); y += 28;
    state.hTts      = MakeCheck(hwnd, L"Enable audio pronunciation (SAPI 5)",
                                x, y, w, IDC_SETTINGS_TTS, cur.ttsEnabled); y += 28;

    MakeLabel(hwnd, L"Bubble auto-hide (ms):", x, y + 4, 180);
    wchar_t buf[32]; swprintf_s(buf, L"%d", cur.bubbleIdleMs);
    state.hIdleMs = MakeEdit(hwnd, buf, x + 190, y, 120, IDC_SETTINGS_BUBBLE_MS); y += 32;

    MakeLabel(hwnd, L"Popup font size:", x, y + 4, 180);
    swprintf_s(buf, L"%d", cur.popupFontSize);
    state.hFontSize = MakeEdit(hwnd, buf, x + 190, y, 120, IDC_SETTINGS_FONTSIZE); y += 48;

    MakeButton(hwnd, L"OK",     x + 120, y, 90, IDC_SETTINGS_OK,     true);
    MakeButton(hwnd, L"Cancel", x + 220, y, 90, IDC_SETTINGS_CANCEL, false);

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    // Local modal loop — runs until the settings window is destroyed. If
    // WM_QUIT arrives (e.g. tray Quit) GetMessage returns 0; we re-post it so
    // the outer app loop also sees the shutdown signal.
    for (;;) {
        MSG msg;
        BOOL r = ::GetMessageW(&msg, nullptr, 0, 0);
        if (r == 0) {
            ::PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (r == -1) break;
        if (!::IsDialogMessageW(hwnd, &msg)) {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
        if (!::IsWindow(hwnd)) break;
    }
    return state.result;
}

} // namespace settings_window
