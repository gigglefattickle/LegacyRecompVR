#include <stdafx.h>
#include <os/logger.h>
#include "storage_android.h"

#include <android/log.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>

namespace
{
    std::mutex g_logMutex;
    int g_logFd = -1;

    const char* SignalDescription(int signalNumber)
    {
        switch (signalNumber)
        {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        case SIGILL:  return "SIGILL";
        default:      return "UNKNOWN";
        }
    }

    void WriteCrashRecord(int signalNumber)
    {
        if (g_logFd < 0)
            return;

        static constexpr char prefix[] = "FATAL NATIVE SIGNAL: ";
        static constexpr char newline[] = "\n";
        const char* description = SignalDescription(signalNumber);
        (void)::write(g_logFd, prefix, sizeof(prefix) - 1);
        (void)::write(g_logFd, description, std::strlen(description));
        (void)::write(g_logFd, newline, sizeof(newline) - 1);
        (void)::fsync(g_logFd);
    }

    void AndroidFatalSignalHandler(int signalNumber, siginfo_t*, void*)
    {
        WriteCrashRecord(signalNumber);
        // SA_RESETHAND restores the default disposition on entry. Re-raise so
        // Android still observes a genuine fatal signal and can create a tombstone.
        (void)::kill(::getpid(), signalNumber);
    }

    void AndroidTerminateHandler()
    {
        static constexpr char message[] = "FATAL: uncaught C++ exception / std::terminate\n";
        if (g_logFd >= 0)
        {
            (void)::write(g_logFd, message, sizeof(message) - 1);
            (void)::fsync(g_logFd);
        }
        std::abort();
    }
}

void os::logger::Init()
{
    (void)os::android::storage::EnsureDirectories();

    const auto path = os::android::storage::GetLogFilePath();
    if (!path.empty())
        g_logFd = ::open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC, 0600);

    struct sigaction action{};
    action.sa_sigaction = AndroidFatalSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_RESETHAND;
    for (const int signalNumber : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL})
        (void)sigaction(signalNumber, &action, nullptr);

    std::set_terminate(AndroidTerminateHandler);
    Log("Android diagnostics initialized", ELogType::Utility, "Init");
}

void os::logger::Log(const std::string_view str, ELogType type, const char* func)
{
    int priority = ANDROID_LOG_INFO;
    switch (type)
    {
    case ELogType::Warning: priority = ANDROID_LOG_WARN; break;
    case ELogType::Error:   priority = ANDROID_LOG_ERROR; break;
    case ELogType::Utility: priority = ANDROID_LOG_DEBUG; break;
    case ELogType::None:
    default: break;
    }

    if (func != nullptr)
        __android_log_print(priority, "LegacyRecomp", "[%s] %.*s", func,
                            static_cast<int>(str.size()), str.data());
    else
        __android_log_print(priority, "LegacyRecomp", "%.*s",
                            static_cast<int>(str.size()), str.data());

    std::lock_guard lock(g_logMutex);
    if (g_logFd >= 0)
    {
        std::string line;
        if (func != nullptr)
            line = "[" + std::string(func) + "] ";
        line.append(str);
        line.push_back('\n');
        (void)::write(g_logFd, line.data(), line.size());
    }
}
