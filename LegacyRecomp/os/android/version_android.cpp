#include <os/version.h>

#include <android/api-level.h>

os::version::OSVersion os::version::GetOSVersion()
{
    os::version::OSVersion version{};
    version.Major = static_cast<uint32_t>(android_get_device_api_level());
    return version;
}
