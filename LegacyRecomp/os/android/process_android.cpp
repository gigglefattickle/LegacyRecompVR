#include <os/process.h>

#include <SDL_system.h>
#include <unistd.h>

std::filesystem::path os::process::GetExecutablePath()
{
    // Android apps execute from an APK; there is no standalone executable path.
    return {};
}

std::filesystem::path os::process::GetWorkingDirectory()
{
    const char* path = SDL_AndroidGetInternalStoragePath();
    return path != nullptr ? std::filesystem::path(path) : std::filesystem::path{};
}

bool os::process::SetWorkingDirectory(const std::filesystem::path& path)
{
    return chdir(path.c_str()) == 0;
}

bool os::process::StartProcess(const std::filesystem::path&,
                              const std::vector<std::string>&,
                              std::filesystem::path)
{
    // Android does not allow an app to spawn an arbitrary executable.
    return false;
}

void os::process::CheckConsole()
{
    g_consoleVisible = false;
}

void os::process::ShowConsole()
{
    // Android has no attached console; use the Android log.
}
