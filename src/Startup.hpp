#pragma once
#include <string>

namespace startup {

// Returns true if the HKCU\...\Run key for "EnViDict" exists.
bool IsEnabled();

// Adds the value pointing at the current executable with --hidden flag.
bool Enable();

// Removes the value if present.
bool Disable();

} // namespace startup
