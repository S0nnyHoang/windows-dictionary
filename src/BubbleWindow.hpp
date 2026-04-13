#pragma once
#include <windows.h>

#include <string>

class App;

// Tiny "D" bubble that appears next to a fresh selection. Click → ask App to
// show the popup. Layered, non-activating, top-most. One-shot auto-hide timer.
class BubbleWindow {
public:
    bool Create(HINSTANCE hInst, App* app);
    void Destroy();

    // anchorScreen is the rectangle of the selection in screen pixels.
    void ShowAt(const RECT& anchorScreen, std::wstring selectionText);
    void Hide();

    HWND Handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    void Paint();
    void OnTimer();

    HWND hwnd_ = nullptr;
    App* app_ = nullptr;
    std::wstring pendingText_;
    POINT lastAnchor_{0, 0};
    static constexpr UINT_PTR kHideTimer = 1;
};
