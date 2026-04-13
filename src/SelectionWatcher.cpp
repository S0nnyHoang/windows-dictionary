#include "SelectionWatcher.hpp"

#include "App.hpp"
#include "UiaEventHandlers.hpp"

#include <UIAutomation.h>

bool SelectionWatcher::Start(App* app, bool enableClipboardFallback) {
    if (running_) return true;
    app_ = app;
    clipboardFallback_ = enableClipboardFallback;

    HRESULT hr = ::CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                    __uuidof(IUIAutomation),
                                    uia_.PutVoid());
    if (FAILED(hr) || !uia_) return false;

    ComPtr<IUIAutomationElement> root;
    if (FAILED(uia_->GetRootElement(root.GetAddressOf())) || !root) return false;

    auto* h = new UiaSelectionHandler(
        [app](const std::wstring& text, RECT anchor) {
            // Runs on a UIA RPC thread — marshal over to the UI thread.
            app->PostSelection(text, anchor);
        });
    handler_ = h;

    hr = uia_->AddAutomationEventHandler(
        UIA_Text_TextSelectionChangedEventId,
        root.Get(),
        TreeScope_Subtree,
        nullptr,
        handler_);
    if (FAILED(hr)) {
        handler_->Release();
        handler_ = nullptr;
        return false;
    }

    if (clipboardFallback_) {
        hook_.Install(&SelectionWatcher::OnMouseDragUp, this);
    }

    running_ = true;
    return true;
}

void SelectionWatcher::Stop() {
    if (!running_) return;
    hook_.Uninstall();
    if (uia_ && handler_) {
        uia_->RemoveAllEventHandlers();
        handler_->Release();
        handler_ = nullptr;
    }
    uia_.Reset();
    running_ = false;
}

void SelectionWatcher::OnMouseDragUp(POINT screen, void* user) {
    auto* self = static_cast<SelectionWatcher*>(user);
    if (!self || !self->app_) return;
    self->app_->PostClipboardProbe(screen);
}
