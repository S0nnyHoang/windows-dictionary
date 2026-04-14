#pragma once

#include "ComPtr.hpp"
#include "Hook.hpp"

#include <windows.h>
#include <UIAutomation.h>

class App;

// Wires up UIA selection-change events and the optional mouse-hook fallback.
class SelectionWatcher {
public:
    bool Start(App* app, bool enableClipboardFallback);
    void Stop();

    bool IsRunning() const { return running_; }

private:
    static void OnMouseDragUp(POINT screen, void* user);

    App* app_ = nullptr;
    bool running_ = false;
    bool clipboardFallback_ = false;

    ComPtr<IUIAutomation> uia_;
    IUIAutomationEventHandler* handler_ = nullptr; // owned by UIA while registered
    MouseHook hook_;
};
