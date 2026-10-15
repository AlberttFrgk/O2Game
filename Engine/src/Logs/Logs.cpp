#include <Logs.h>
#include <cstdarg>
#include <stdio.h>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

#include "Console.h"
#include "Misc/CrashHandler.h"

namespace {
    std::mutex g_fileMutex;
    std::ofstream g_gameLogFile;
    bool g_logFileInitialized = false;

    void EnsureLogFileOpen()
    {
        if (!g_logFileInitialized) {
            g_gameLogFile.open("game.log", std::ios::out | std::ios::trunc);
            g_logFileInitialized = true;
        }
    }
}

void Logs::Flush()
{
    std::lock_guard<std::mutex> lock(g_fileMutex);
    if (g_logFileInitialized && g_gameLogFile.is_open()) {
        g_gameLogFile.flush();
    }
}

void Logs::Puts(const char *fmt, ...)
{
    char buffer[2048] = {};

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    ::puts(buffer);
    Console::Send(buffer);

    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tmNow{};
#if _WIN32
    localtime_s(&tmNow, &in_time_t);
#else
    localtime_r(&in_time_t, &tmNow);
#endif
    std::ostringstream ss;
    ss << "[" << std::put_time(&tmNow, "%Y-%m-%d %H:%M:%S") << "] " << buffer;
    std::string timestamped = ss.str();

    CrashHandler::AddLogMessage(timestamped);

    std::lock_guard<std::mutex> lock(g_fileMutex);
    EnsureLogFileOpen();
    if (g_gameLogFile.is_open()) {
        g_gameLogFile << timestamped << "\n";
        g_gameLogFile.flush();
    }
}

void Logs::Write(const char *tag, const char *fmt, ...)
{
    char msg[1800] = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (tag && *tag) {
        Puts("[%s] %s", tag, msg);
    } else {
        Puts("%s", msg);
    }
}