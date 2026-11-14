#pragma once

#include <filesystem>

namespace os::android::storage
{
    // Android app-private directories; these do not require broad storage permissions.
    std::filesystem::path GetAppFilesDirectory();
    std::filesystem::path GetCacheDirectory();
    std::filesystem::path GetGameDirectory();
    std::filesystem::path GetSaveDirectory();
    std::filesystem::path GetConfigDirectory();
    std::filesystem::path GetImportDirectory();
    std::filesystem::path GetLogFilePath();

    bool EnsureDirectories();
    bool IsReadableFile(const std::filesystem::path& path);
}
