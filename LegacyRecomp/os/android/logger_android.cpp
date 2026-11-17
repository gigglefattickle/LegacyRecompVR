#include <os/logger.h>
#include <SDL.h>

void os::logger::Init()
{
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_VERBOSE);
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    SDL_LogPriority priority = SDL_LOG_PRIORITY_INFO;
    switch (type)
    {
        case ELogType::Utility: priority = SDL_LOG_PRIORITY_VERBOSE; break;
        case ELogType::Warning: priority = SDL_LOG_PRIORITY_WARN; break;
        case ELogType::Error: priority = SDL_LOG_PRIORITY_ERROR; break;
        default: break;
    }

    if (func)
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "[%s] %.*s", func, static_cast<int>(str.size()), str.data());
    else
        SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, priority, "%.*s", static_cast<int>(str.size()), str.data());
}
