#include "UiaEventHandlers.hpp"

#include "ComPtr.hpp"

#include <OleAuto.h>

IFACEMETHODIMP UiaSelectionHandler::HandleAutomationEvent(IUIAutomationElement* sender,
                                                          EVENTID eventId) {
    if (!sender || eventId != UIA_Text_TextSelectionChangedEventId || !cb_) {
        return S_OK;
    }

    ComPtr<IUnknown> patternUnk;
    if (FAILED(sender->GetCurrentPattern(UIA_TextPatternId, patternUnk.GetAddressOf()))
        || !patternUnk) {
        return S_OK;
    }
    ComPtr<IUIAutomationTextPattern> textPattern;
    if (FAILED(patternUnk->QueryInterface(__uuidof(IUIAutomationTextPattern),
                                          textPattern.PutVoid())) || !textPattern) {
        return S_OK;
    }

    ComPtr<IUIAutomationTextRangeArray> ranges;
    if (FAILED(textPattern->GetSelection(ranges.GetAddressOf())) || !ranges) {
        return S_OK;
    }
    int count = 0;
    ranges->get_Length(&count);
    if (count <= 0) return S_OK;

    ComPtr<IUIAutomationTextRange> range;
    if (FAILED(ranges->GetElement(0, range.GetAddressOf())) || !range) return S_OK;

    BSTR bstr = nullptr;
    if (FAILED(range->GetText(256, &bstr)) || !bstr) return S_OK;
    std::wstring text(bstr, ::SysStringLen(bstr));
    ::SysFreeString(bstr);

    RECT anchor{0, 0, 0, 0};
    SAFEARRAY* sa = nullptr;
    if (SUCCEEDED(range->GetBoundingRectangles(&sa)) && sa) {
        LONG lo = 0, hi = 0;
        ::SafeArrayGetLBound(sa, 1, &lo);
        ::SafeArrayGetUBound(sa, 1, &hi);
        if (hi - lo + 1 >= 4) {
            double vals[4] = {0, 0, 0, 0};
            for (LONG i = 0; i < 4; ++i) {
                LONG idx = lo + i;
                ::SafeArrayGetElement(sa, &idx, &vals[i]);
            }
            anchor.left   = static_cast<LONG>(vals[0]);
            anchor.top    = static_cast<LONG>(vals[1]);
            anchor.right  = static_cast<LONG>(vals[0] + vals[2]);
            anchor.bottom = static_cast<LONG>(vals[1] + vals[3]);
        }
        ::SafeArrayDestroy(sa);
    }

    cb_(text, anchor);
    return S_OK;
}
