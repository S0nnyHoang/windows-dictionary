#include "BubbleWindow.hpp"

#include "App.hpp"
#include "DpiUtil.hpp"

namespace {
constexpr const wchar_t* kClass = L"EnViDictBubble";
constexpr int kSizeDip = 22;
}

LRESULT CALLBACK BubbleWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    BubbleWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<BubbleWindow*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<BubbleWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return ::DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_LBUTTONUP: {
        // Tell the app to look this word up.
        if (self->app_) self->app_->OnBubbleClicked(self->pendingText_, self->lastAnchor_);
        self->Hide();
        return 0;
    }
    case WM_TIMER:
        if (wp == kHideTimer) {
            self->OnTimer();
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        ::BeginPaint(hwnd, &ps);
        self->Paint();
        ::EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        self->hwnd_ = nullptr;
        return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

bool BubbleWindow::Create(HINSTANCE hInst, App* app) {
    app_ = app;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = BubbleWindow::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_HAND);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClass;
    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kClass, L"", WS_POPUP,
        0, 0, 32, 32,
        nullptr, nullptr, hInst, this);

    if (!hwnd_) return false;
    ::SetLayeredWindowAttributes(hwnd_, 0, 230, LWA_ALPHA);
    return true;
}

void BubbleWindow::Destroy() {
    if (hwnd_) {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void BubbleWindow::ShowAt(const RECT& anchorScreen, std::wstring selectionText) {
    if (!hwnd_) return;
    pendingText_ = std::move(selectionText);

    const UINT dpi = dpi::GetForWindow(hwnd_);
    const int sz = dpi::Scale(kSizeDip, dpi);

    // Place to the right of, slightly below, the selection's top-right corner.
    int x = anchorScreen.right + dpi::Scale(2, dpi);
    int y = anchorScreen.bottom + dpi::Scale(2, dpi);

    RECT mr{};
    HMONITOR mon = ::MonitorFromPoint({x, y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (::GetMonitorInfoW(mon, &mi)) {
        mr = mi.rcWork;
        if (x + sz > mr.right)  x = mr.right - sz - 1;
        if (y + sz > mr.bottom) y = mr.bottom - sz - 1;
        if (x < mr.left)        x = mr.left;
        if (y < mr.top)         y = mr.top;
    }
    lastAnchor_ = { x, y };

    ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, sz, sz, SWP_NOACTIVATE);
    ::ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
    ::InvalidateRect(hwnd_, nullptr, TRUE);

    ::KillTimer(hwnd_, kHideTimer);
    int idleMs = 4000;
    if (app_) idleMs = app_->BubbleIdleMs();
    ::SetTimer(hwnd_, kHideTimer, static_cast<UINT>(idleMs), nullptr);
}

void BubbleWindow::Hide() {
    if (!hwnd_) return;
    ::KillTimer(hwnd_, kHideTimer);
    ::ShowWindow(hwnd_, SW_HIDE);
}

void BubbleWindow::OnTimer() {
    Hide();
}

void BubbleWindow::Paint() {
    if (!hwnd_) return;
    RECT rc;
    ::GetClientRect(hwnd_, &rc);

    HDC hdc = ::GetDC(hwnd_);
    HDC mdc = ::CreateCompatibleDC(hdc);
    HBITMAP bmp = ::CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ old = ::SelectObject(mdc, bmp);

    HBRUSH bg = ::CreateSolidBrush(RGB(45, 116, 232));
    ::FillRect(mdc, &rc, bg);
    ::DeleteObject(bg);

    ::SetBkMode(mdc, TRANSPARENT);
    ::SetTextColor(mdc, RGB(255, 255, 255));
    HFONT font = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
    HGDIOBJ oldFont = ::SelectObject(mdc, font);
    ::DrawTextW(mdc, L"D", 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    ::SelectObject(mdc, oldFont);

    ::BitBlt(hdc, 0, 0, rc.right, rc.bottom, mdc, 0, 0, SRCCOPY);

    ::SelectObject(mdc, old);
    ::DeleteObject(bmp);
    ::DeleteDC(mdc);
    ::ReleaseDC(hwnd_, hdc);
}
