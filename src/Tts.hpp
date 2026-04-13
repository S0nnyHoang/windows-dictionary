#pragma once

#include "ComPtr.hpp"

#include <sapi.h>
#include <string>
#include <string_view>

class Tts {
public:
    bool Initialize();          // enumerates voices, picks English, sets rate
    bool IsAvailable() const { return voice_ != nullptr; }
    void Speak(std::wstring_view text);
    void Shutdown();

private:
    ComPtr<ISpVoice> voice_;
};
