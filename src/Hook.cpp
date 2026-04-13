#include "Hook.hpp"

MouseHook* MouseHook::s_instance = nullptr;

LRESULT CALLBACK MouseHook::LowLevelProc(int code, WPARAM wp, LPARAM lp) {
    if (code == HC_ACTION && s_instance) {
        auto* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lp);
        if (wp == WM_LBUTTONDOWN) {
            s_instance->downPos_ = info->pt;
            s_instance->downSeen_ = true;
        } else if (wp == WM_LBUTTONUP && s_instance->downSeen_) {
            s_instance->downSeen_ = false;
            LONG dx = info->pt.x - s_instance->downPos_.x;
            LONG dy = info->pt.y - s_instance->downPos_.y;
            if (dx * dx + dy * dy >= 16) { // moved ≥ 4 px → probably a drag-select
                if (s_instance->cb_) {
                    s_instance->cb_(info->pt, s_instance->userData_);
                }
            }
        }
    }
    return ::CallNextHookEx(nullptr, code, wp, lp);
}

bool MouseHook::Install(Callback cb, void* userData) {
    if (hook_) return true;
    s_instance = this;
    cb_ = cb;
    userData_ = userData;
    hook_ = ::SetWindowsHookExW(WH_MOUSE_LL, &MouseHook::LowLevelProc,
                                ::GetModuleHandleW(nullptr), 0);
    return hook_ != nullptr;
}

void MouseHook::Uninstall() {
    if (hook_) {
        ::UnhookWindowsHookEx(hook_);
        hook_ = nullptr;
    }
    if (s_instance == this) s_instance = nullptr;
}
