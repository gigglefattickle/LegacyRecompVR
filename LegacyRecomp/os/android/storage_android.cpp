#include <stdafx.h>
#include "storage_android.h"

#include <SDL_system.h>

namespace
{
    std::filesystem::path GetPrivateRoot()
    {
        const char* path = SDL_AndroidGetInternalStoragePath();
        if (path == nullptr || path[0] == '\0')
            return {};

        return std::filesystem::path(path);
    }
}

std::filesystem::path os::android::storage::GetAppFilesDirectory()
{
    return GetPrivateRoot();
}

std::filesystem::path os::android::storage::GetCacheDirectory()
{
    const char* path = SDL_AndroidGetInternalStoragePath();
    if (path == nullptr || path[0] == '\0')
        return {};
    return std::filesystem::path(path) / "cache";
}

std::filesystem::path os::android::storage::GetGameDirectory()
{
    return GetAppFilesDirectory() / "game";
}

std::filesystem::path os::android::storage::GetSaveDirectory()
{
    return GetAppFilesDirectory() / "saves";
}

std::filesystem::path os::android::storage::GetConfigDirectory()
{
    return GetAppFilesDirectory() / "config";
}

std::filesystem::path os::android::storage::GetImportDirectory()
{
    return GetAppFilesDirectory() / "imports";
}

std::filesystem::path os::android::storage::GetLogFilePath()
{
    return GetAppFilesDirectory() / "logs" / "legacyrecomp.log";
}

bool os::android::storage::EnsureDirectories()
{
    const auto root = GetAppFilesDirectory();
    if (root.empty())
        return false;

    std::error_code ec;
    for (const auto& directory : {
             root,
             GetCacheDirectory(),
             GetGameDirectory(),
             GetSaveDirectory(),
             GetConfigDirectory(),
             GetImportDirectory(),
             GetLogFilePath().parent_path()})
    {
        if (directory.empty())
            return false;
        std::filesystem::create_directories(directory, ec);
        if (ec)
            return false;
    }
    return true;
}

bool os::android::storage::IsReadableFile(const std::filesystem::path& path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec) && !ec &&
           std::filesystem::exists(path, ec) && !ec;
}
