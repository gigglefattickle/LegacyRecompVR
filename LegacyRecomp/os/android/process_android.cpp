#include <os/process.h>

#include <SDL.h>
#include <SDL_system.h>

#include <unistd.h>
#include <limits.h>

std::filesystem::path os::process::GetExecutablePath()
{
    // SDL Android apps run inside app_process; there is no standalone game executable.
    // Return a stable app-private sentinel so the shared startup code resolves its
    // working directory to SDL's internal storage rather than /system/bin.
    const char* internalPath = SDL_AndroidGetInternalStoragePath();
    if (!internalPath || !*internalPath)
        return {};
    return std::filesystem::path(internalPath) / "LegacyRecomp";
}

std::filesystem::path os::process::GetWorkingDirectory()
{
    char cwd[PATH_MAX] = {};
    if (getcwd(cwd, sizeof(cwd)))
        return std::filesystem::path(cwd);
    const char* internalPath = SDL_AndroidGetInternalStoragePath();
    return internalPath ? std::filesystem::path(internalPath) : std::filesystem::path();
}

bool os::process::SetWorkingDirectory(const std::filesystem::path& path)
{
    return chdir(path.c_str()) == 0;
}

bool os::process::StartProcess(const std::filesystem::path&, const std::vector<std::string>&, std::filesystem::path)
{
    // Android does not support relaunching this native activity as a child process.
    return false;
}

void os::process::CheckConsole()
{
    g_consoleVisible = false;
}

void os::process::ShowConsole()
{
    // Diagnostics are sent to logcat through SDL_Log.
}
