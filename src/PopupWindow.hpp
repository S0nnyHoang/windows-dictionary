#pragma once
#include <windows.h>

#include "ComPtr.hpp"
#include "Dictionary.hpp"

#include <d2d1_1.h>
#include <dwrite_1.h>
#include <memory>

class App;
class Renderer;

class PopupWindow {
public:
    bool Create(HINSTANCE hInst, App* app, Renderer* renderer);
    void Destroy();

    void ShowAt(POINT anchorScreen, std::unique_ptr<LookupResult> result);
    void Hide();

    HWND Handle() const { return hwnd_; }

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    void EnsureD2DTarget();
    void ReleaseD2DResources();
    void Paint();
    void Layout();
    SIZE PreferredSize() const;
    void SpeakerHitTest(POINT pt, bool& hit);

    HWND hwnd_ = nullptr;
    App* app_ = nullptr;
    Renderer* renderer_ = nullptr;

    std::unique_ptr<LookupResult> result_;
    ComPtr<IDWriteTextLayout> layout_;

    ComPtr<ID2D1HwndRenderTarget> rt_;
    ComPtr<ID2D1SolidColorBrush>  brushText_;
    ComPtr<ID2D1SolidColorBrush>  brushBg_;
    ComPtr<ID2D1SolidColorBrush>  brushAccent_;

    RECT speakerRect_{};
    bool speakerEnabled_ = false;
};
