#include "Tts.hpp"

#include <sphelper.h>
#include <string>

#pragma comment(lib, "sapi.lib")

bool Tts::Initialize() {
    HRESULT hr = ::CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_ISpVoice, voice_.PutVoid());
    if (FAILED(hr) || !voice_) return false;

    // Try to select an English voice; fall back to default.
    ComPtr<IEnumSpObjectTokens> e;
    if (SUCCEEDED(SpEnumTokens(SPCAT_VOICES, L"Language=409", nullptr,
                               e.GetAddressOf())) && e) {
        ComPtr<ISpObjectToken> tok;
        ULONG fetched = 0;
        if (SUCCEEDED(e->Next(1, tok.GetAddressOf(), &fetched)) && fetched) {
            voice_->SetVoice(tok.Get());
            return true;
        }
    }
    if (SUCCEEDED(SpEnumTokens(SPCAT_VOICES, L"Language=809", nullptr,
                               e.ReleaseAndGetAddressOf())) && e) {
        ComPtr<ISpObjectToken> tok;
        ULONG fetched = 0;
        if (SUCCEEDED(e->Next(1, tok.GetAddressOf(), &fetched)) && fetched) {
            voice_->SetVoice(tok.Get());
            return true;
        }
    }
    // No English voice present — treat the subsystem as unavailable.
    voice_.Reset();
    return false;
}

void Tts::Speak(std::wstring_view text) {
    if (!voice_ || text.empty()) return;
    std::wstring s(text);
    voice_->Speak(s.c_str(), SPF_ASYNC | SPF_PURGEBEFORESPEAK, nullptr);
}

void Tts::Shutdown() {
    voice_.Reset();
}
