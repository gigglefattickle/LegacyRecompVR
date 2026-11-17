#include <os/version.h>

#include <android/api-level.h>

os::version::OSVersion os::version::GetOSVersion()
{
    // Android exposes a platform API level rather than a Windows-style build tuple.
    // Store it in Build and leave Major/Minor zero to avoid inventing a release version.
    const int apiLevel = android_get_device_api_level();
    return { 0, 0, apiLevel > 0 ? static_cast<uint32_t>(apiLevel) : 0u };
}
