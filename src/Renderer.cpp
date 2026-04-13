#include "Renderer.hpp"

#include "Normalize.hpp"

#include <d2d1.h>
#include <dwrite.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

bool Renderer::EnsureFactories() {
    if (d2d_ && dwrite_) return true;
    if (!d2d_) {
        D2D1_FACTORY_OPTIONS opts{};
        HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                                       __uuidof(ID2D1Factory1), &opts,
                                       d2d_.PutVoid());
        if (FAILED(hr)) return false;
    }
    if (!dwrite_) {
        IDWriteFactory* base = nullptr;
        HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                         __uuidof(IDWriteFactory1),
                                         reinterpret_cast<IUnknown**>(&base));
        if (FAILED(hr)) return false;
        dwrite_.Attach(static_cast<IDWriteFactory1*>(base));
    }
    return true;
}

IDWriteTextLayout* Renderer::BuildLayout(const std::vector<ir::TextRun>& runs,
                                         const std::wstring& headword,
                                         float maxWidth, float fontSize) {
    if (!EnsureFactories()) return nullptr;

    // Compose a single string with style ranges to apply afterwards.
    std::wstring text;
    struct Range { UINT32 start; UINT32 length; uint8_t style; };
    std::vector<Range> ranges;

    if (!headword.empty()) {
        Range r{ static_cast<UINT32>(text.size()), 0, ir::StyleBold };
        text.append(headword);
        text.append(L"\n");
        r.length = static_cast<UINT32>(headword.size());
        ranges.push_back(r);
    }

    for (const auto& run : runs) {
        if (run.text.empty()) continue;
        if (run.style & ir::StyleNewlineBefore) text.append(L"\n");
        Range r{ static_cast<UINT32>(text.size()), 0, run.style };
        std::wstring w = normalize::FromUtf8(run.text);
        text.append(w);
        r.length = static_cast<UINT32>(w.size());
        ranges.push_back(r);
    }

    if (text.empty()) text = L"(no definition)";

    ComPtr<IDWriteTextFormat> format;
    HRESULT hr = dwrite_->CreateTextFormat(
        L"Segoe UI", nullptr,
        DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        fontSize, L"en-us", format.GetAddressOf());
    if (FAILED(hr)) return nullptr;
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);

    IDWriteTextLayout* layout = nullptr;
    hr = dwrite_->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.size()),
                                   format.Get(), maxWidth, 4096.0f, &layout);
    if (FAILED(hr)) return nullptr;

    for (const auto& r : ranges) {
        DWRITE_TEXT_RANGE range{ r.start, r.length };
        if (r.style & ir::StyleBold) {
            layout->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD, range);
        }
        if (r.style & ir::StyleItalic) {
            layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
        }
        if (r.style & ir::StylePosTag) {
            layout->SetFontStyle(DWRITE_FONT_STYLE_ITALIC, range);
        }
    }

    return layout;
}
