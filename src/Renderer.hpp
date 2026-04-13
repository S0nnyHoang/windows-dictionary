#pragma once

#include "ComPtr.hpp"
#include "Ir.hpp"

#include <d2d1_1.h>
#include <dwrite_1.h>
#include <wincodec.h>

#include <string>
#include <vector>

// Wraps Direct2D / DirectWrite. Other modules never include the D2D headers.
// One Renderer instance is shared by BubbleWindow and PopupWindow; per-window
// render targets are created lazily and released on hide.
class Renderer {
public:
    bool EnsureFactories();

    // Builds a DirectWrite text layout from a sequence of styled runs.
    // Returns nullptr on failure. Caller owns the layout.
    IDWriteTextLayout* BuildLayout(const std::vector<ir::TextRun>& runs,
                                   const std::wstring& headword,
                                   float maxWidth, float fontSize);

    IDWriteFactory1* DWrite() { return dwrite_.Get(); }
    ID2D1Factory1*   D2D()    { return d2d_.Get(); }

private:
    ComPtr<ID2D1Factory1>  d2d_;
    ComPtr<IDWriteFactory1> dwrite_;
};
