#include "App.hpp"

#include <windows.h>

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int) {
    // Single-instance guard — if another EnViDict is already running, activate
    // the existing one's settings window and exit.
    HANDLE mutex = ::CreateMutexW(nullptr, FALSE, L"Local\\EnViDict_SingleInstance");
    if (mutex && ::GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = ::FindWindowW(L"EnViDictMessageWindow", L"");
        if (existing) ::PostMessageW(existing, WM_APP + 4 /*WM_APP_SHOW_SETTINGS*/, 0, 0);
        return 0;
    }

    App app;
    if (!app.Initialize(hInst)) return 1;
    int rc = app.Run();
    app.Shutdown();
    if (mutex) ::CloseHandle(mutex);
    return rc;
}
