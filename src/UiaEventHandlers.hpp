#pragma once

#include <windows.h>
#include <objbase.h>
#include <UIAutomation.h>
#include <atomic>
#include <functional>
#include <string>

// IUIAutomationEventHandler impl that forwards selection-changed events to a
// plain callback. The callback runs on the UIA RPC thread — it must only do
// cheap work and marshal to the UI thread via PostMessage.
class UiaSelectionHandler : public IUIAutomationEventHandler {
public:
    // Callback receives the selection text and bounding rectangle in screen px.
    using Callback = std::function<void(const std::wstring&, RECT)>;

    explicit UiaSelectionHandler(Callback cb) : cb_(std::move(cb)) {}

    // IUnknown
    IFACEMETHODIMP_(ULONG) AddRef() override  { return ++ref_; }
    IFACEMETHODIMP_(ULONG) Release() override {
        ULONG c = --ref_;
        if (c == 0) delete this;
        return c;
    }
    IFACEMETHODIMP QueryInterface(REFIID iid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (iid == __uuidof(IUnknown) ||
            iid == __uuidof(IUIAutomationEventHandler)) {
            *ppv = static_cast<IUIAutomationEventHandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    // IUIAutomationEventHandler
    IFACEMETHODIMP HandleAutomationEvent(IUIAutomationElement* sender, EVENTID eventId) override;

private:
    std::atomic<ULONG> ref_{1};
    Callback cb_;
};
