#include "PopupWindow.hpp"

#include "App.hpp"
#include "DpiUtil.hpp"
#include "Renderer.hpp"

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif

namespace {
constexpr const wchar_t* kClass = L"EnViDictPopup";
constexpr int kMinWidthDip   = 360;
constexpr int kMaxWidthDip   = 460;
constexpr int kMinHeightDip  = 100;
constexpr int kMaxHeightDip  = 360;
constexpr int kPaddingDip    = 14;
constexpr int kSpeakerSizeDip= 22;
}

LRESULT CALLBACK PopupWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PopupWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<PopupWindow*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<PopupWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (!self) return ::DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        ::BeginPaint(hwnd, &ps);
        self->Paint();
        ::EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONUP: {
        POINT pt{ LOWORD(lp), HIWORD(lp) };
        bool hit = false;
        self->SpeakerHitTest(pt, hit);
        if (hit && self->app_ && self->result_) {
            self->app_->Speak(self->result_->headword.empty()
                ? self->result_->sourceTerm
                : self->result_->headword);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            self->Hide();
            return 0;
        }
        break;
    case WM_KILLFOCUS:
        // Don't auto-dismiss on focus loss because we never take focus.
        return 0;
    case WM_DPICHANGED: {
        auto* nr = reinterpret_cast<RECT*>(lp);
        ::SetWindowPos(hwnd, nullptr, nr->left, nr->top,
                       nr->right - nr->left, nr->bottom - nr->top,
                       SWP_NOACTIVATE | SWP_NOZORDER);
        self->ReleaseD2DResources();
        ::InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_DESTROY:
        self->ReleaseD2DResources();
        self->hwnd_ = nullptr;
        return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

bool PopupWindow::Create(HINSTANCE hInst, App* app, Renderer* renderer) {
    app_ = app;
    renderer_ = renderer;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = PopupWindow::WndProc;
    wc.hInstance = hInst;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = kClass;
    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClass, L"EnViDict", WS_POPUP,
        0, 0, 100, 100,
        nullptr, nullptr, hInst, this);
    if (!hwnd_) return false;

    DWORD pref = DWMWCP_ROUND;
    ::DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    return true;
}

void PopupWindow::Destroy() {
    if (hwnd_) {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void PopupWindow::EnsureD2DTarget() {
    if (rt_ || !renderer_ || !hwnd_) return;
    if (!renderer_->EnsureFactories()) return;

    RECT rc;
    ::GetClientRect(hwnd_, &rc);
    D2D1_SIZE_U sz = D2D1::SizeU(static_cast<UINT32>(rc.right),
                                 static_cast<UINT32>(rc.bottom));

    auto rtProps = D2D1::RenderTargetProperties();
    auto hwndProps = D2D1::HwndRenderTargetProperties(hwnd_, sz, D2D1_PRESENT_OPTIONS_NONE);
    HRESULT hr = renderer_->D2D()->CreateHwndRenderTarget(rtProps, hwndProps,
                                                          rt_.GetAddressOf());
    if (FAILED(hr)) return;
    rt_->CreateSolidColorBrush(D2D1::ColorF(0xF7F7FA), brushBg_.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(0x202028), brushText_.GetAddressOf());
    rt_->CreateSolidColorBrush(D2D1::ColorF(0x2D74E8), brushAccent_.GetAddressOf());
}

void PopupWindow::ReleaseD2DResources() {
    layout_.Reset();
    brushBg_.Reset();
    brushText_.Reset();
    brushAccent_.Reset();
    rt_.Reset();
}

SIZE PopupWindow::PreferredSize() const {
    UINT dpi = dpi::GetForWindow(hwnd_);
    int w = dpi::Scale(kMaxWidthDip,  dpi);
    int h = dpi::Scale(kMinHeightDip, dpi);
    if (layout_) {
        DWRITE_TEXT_METRICS m{};
        layout_->GetMetrics(&m);
        int padding = dpi::Scale(kPaddingDip, dpi) * 2;
        int contentH = static_cast<int>(m.height) + padding + dpi::Scale(kSpeakerSizeDip + 8, dpi);
        h = (contentH < dpi::Scale(kMinHeightDip, dpi))
              ? dpi::Scale(kMinHeightDip, dpi)
              : ((contentH > dpi::Scale(kMaxHeightDip, dpi))
                  ? dpi::Scale(kMaxHeightDip, dpi)
                  : contentH);
    }
    return { w, h };
}

void PopupWindow::Layout() {
    if (!hwnd_ || !renderer_) return;
    if (!result_) return;
    UINT dpi = dpi::GetForWindow(hwnd_);
    float maxWidth = static_cast<float>(dpi::Scale(kMaxWidthDip - kPaddingDip * 2, dpi));
    float fs = 14.0f;
    if (app_) fs = static_cast<float>(app_->PopupFontSize());

    IDWriteTextLayout* raw = renderer_->BuildLayout(
        result_->runs, result_->headword.empty() ? result_->sourceTerm : result_->headword,
        maxWidth, fs);
    layout_.Attach(raw);
}

void PopupWindow::ShowAt(POINT anchorScreen, std::unique_ptr<LookupResult> result) {
    if (!hwnd_) return;
    result_ = std::move(result);

    Layout();
    SIZE sz = PreferredSize();

    int x = anchorScreen.x;
    int y = anchorScreen.y + 20;

    HMONITOR mon = ::MonitorFromPoint({x, y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{ sizeof(mi) };
    if (::GetMonitorInfoW(mon, &mi)) {
        if (x + sz.cx > mi.rcWork.right)  x = mi.rcWork.right - sz.cx;
        if (y + sz.cy > mi.rcWork.bottom) y = anchorScreen.y - sz.cy - 8;
        if (x < mi.rcWork.left) x = mi.rcWork.left;
        if (y < mi.rcWork.top)  y = mi.rcWork.top;
    }

    ::SetWindowPos(hwnd_, HWND_TOPMOST, x, y, sz.cx, sz.cy, SWP_NOACTIVATE);
    ::ShowWindow(hwnd_, SW_SHOWNOACTIVATE);

    if (rt_) rt_->Resize(D2D1::SizeU(static_cast<UINT32>(sz.cx),
                                     static_cast<UINT32>(sz.cy)));
    ::InvalidateRect(hwnd_, nullptr, TRUE);
}

void PopupWindow::Hide() {
    if (!hwnd_) return;
    ::ShowWindow(hwnd_, SW_HIDE);
    // Reclaim graphics memory between showings.
    ReleaseD2DResources();
    result_.reset();
}

void PopupWindow::SpeakerHitTest(POINT pt, bool& hit) {
    hit = (pt.x >= speakerRect_.left && pt.x < speakerRect_.right &&
           pt.y >= speakerRect_.top  && pt.y < speakerRect_.bottom);
}

void PopupWindow::Paint() {
    EnsureD2DTarget();
    if (!rt_) return;

    rt_->BeginDraw();
    rt_->Clear(D2D1::ColorF(0xF7F7FA));

    RECT rc;
    ::GetClientRect(hwnd_, &rc);
    UINT dpi = dpi::GetForWindow(hwnd_);
    float pad = static_cast<float>(dpi::Scale(kPaddingDip, dpi));

    if (layout_) {
        rt_->DrawTextLayout(D2D1::Point2F(pad, pad), layout_.Get(),
                            brushText_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
    } else {
        // Build it now (first paint after Show).
        Layout();
        if (layout_) {
            rt_->DrawTextLayout(D2D1::Point2F(pad, pad), layout_.Get(),
                                brushText_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
        }
    }

    // Speaker button (bottom-right).
    if (app_ && app_->TtsAvailable() && result_ && result_->found) {
        int sz = dpi::Scale(kSpeakerSizeDip, dpi);
        speakerRect_.right  = rc.right - static_cast<LONG>(pad);
        speakerRect_.bottom = rc.bottom - static_cast<LONG>(pad);
        speakerRect_.left   = speakerRect_.right - sz;
        speakerRect_.top    = speakerRect_.bottom - sz;
        D2D1_RECT_F r = D2D1::RectF(
            static_cast<float>(speakerRect_.left),
            static_cast<float>(speakerRect_.top),
            static_cast<float>(speakerRect_.right),
            static_cast<float>(speakerRect_.bottom));
        rt_->FillRoundedRectangle(D2D1::RoundedRect(r, 4.0f, 4.0f), brushAccent_.Get());
        speakerEnabled_ = true;
    } else {
        speakerEnabled_ = false;
        speakerRect_ = {};
    }

    HRESULT hr = rt_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        ReleaseD2DResources();
    }
}
