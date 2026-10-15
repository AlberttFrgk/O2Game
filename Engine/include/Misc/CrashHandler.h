#pragma once
#include <string>

namespace CrashHandler {
    void Install();
    void Uninstall();
    void WriteCrashReport(const std::string &reason, void *exceptionPointers = nullptr);
    void AddLogMessage(const std::string &message);
    std::string GetLastCrashLogPath();
    std::string GetLastCrashDumpPath();
}
