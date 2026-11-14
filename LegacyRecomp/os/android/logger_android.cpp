#include <os/logger.h>

#include <android/log.h>

void os::logger::Init()
{
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    int priority = ANDROID_LOG_INFO;
    switch (type)
    {
    case ELogType::Warning:
        priority = ANDROID_LOG_WARN;
        break;
    case ELogType::Error:
        priority = ANDROID_LOG_ERROR;
        break;
    case ELogType::Utility:
        priority = ANDROID_LOG_DEBUG;
        break;
    case ELogType::None:
    default:
        break;
    }

    if (func != nullptr)
        __android_log_print(priority, "LegacyRecomp", "[%s] %.*s", func,
                            static_cast<int>(str.size()), str.data());
    else
        __android_log_print(priority, "LegacyRecomp", "%.*s",
                            static_cast<int>(str.size()), str.data());
}
