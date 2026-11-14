#include <os/process.h>
#include "storage_android.h"

#include <SDL_system.h>
#include <unistd.h>

std::filesystem::path os::process::GetExecutablePath()
{
    // Android apps execute from an APK; there is no standalone executable path.
    return {};
}

std::filesystem::path os::process::GetWorkingDirectory()
{
    (void)os::android::storage::EnsureDirectories();
    return os::android::storage::GetAppFilesDirectory();
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
