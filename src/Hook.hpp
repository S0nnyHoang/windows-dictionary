#pragma once
#include <windows.h>

// Global low-level mouse hook used as the fallback trigger when UI Automation
// doesn't deliver a text-selection event. The callback runs on the thread
// that installed the hook, so we install from the UI thread and do the
// absolute minimum in the callback: post a window message.
class MouseHook {
public:
    // onLeftUpAfterDrag is called synchronously from the hook callback.
    // It receives the screen point where the button came up.
    using Callback = void(*)(POINT, void*);

    bool Install(Callback cb, void* userData);
    void Uninstall();

private:
    static LRESULT CALLBACK LowLevelProc(int code, WPARAM wp, LPARAM lp);
    HHOOK hook_ = nullptr;
    static MouseHook* s_instance;
    Callback cb_ = nullptr;
    void* userData_ = nullptr;
    POINT downPos_{0, 0};
    bool  downSeen_ = false;
};
