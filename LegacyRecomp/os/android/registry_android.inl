#include <os/registry.h>

inline bool os::registry::Init()
{
    // Android has no Win32-style registry. Persisted settings use the app config.
    return false;
}

template<typename T>
bool os::registry::ReadValue(const std::string_view&, T&)
{
    return false;
}

template<typename T>
bool os::registry::WriteValue(const std::string_view&, const T&)
{
    return false;
}
