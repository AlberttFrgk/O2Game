#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#endif

// STD Headers
#include <algorithm>
#include <filesystem>
#include <iostream>

#if __linux__
#include <unistd.h>
#endif

// Game Headers
#include "./Data/Util/Util.hpp"
#include "./Data/bms.hpp"
#include "./Data/Chart.hpp"
#include "./Resources/DefaultConfiguration.h"
#include "EnvironmentSetup.hpp"
#include "MyGame.h"

// Engine Headers
#include "Configuration.h"
#include "Misc/CrashHandler.h"
#include "MsgBox.h"
#include <Logs.h>

#if _WIN32
#include <windows.h>

extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
    __declspec(dllexport) DWORD IntelGpuPowerPreference = 0x00000002;
}

struct SingleInstanceLock {
    HANDLE m_handle = NULL;
    bool m_alreadyRunning = false;

    SingleInstanceLock(const wchar_t *name) {
        m_handle = CreateMutexW(NULL, TRUE, name);
        if (m_handle == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
            m_alreadyRunning = true;
            if (m_handle != NULL) {
                CloseHandle(m_handle);
                m_handle = NULL;
            }
        }
    }

    ~SingleInstanceLock() {
        if (m_handle != NULL) {
            ReleaseMutex(m_handle);
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

    bool IsAlreadyRunning() const {
        return m_alreadyRunning;
    }
};
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

struct SingleInstanceLock {
    int m_fd = -1;
    bool m_alreadyRunning = false;

    SingleInstanceLock(const char *name) {
        std::string path = "/tmp/" + std::string(name) + ".lock";
        m_fd = open(path.c_str(), O_CREAT | O_RDWR, 0666);
        if (m_fd != -1) {
            if (flock(m_fd, LOCK_EX | LOCK_NB) != 0) {
                m_alreadyRunning = true;
                close(m_fd);
                m_fd = -1;
            }
        }
    }

    ~SingleInstanceLock() {
        if (m_fd != -1) {
            flock(m_fd, LOCK_UN);
            close(m_fd);
            m_fd = -1;
        }
    }

    bool IsAlreadyRunning() const {
        return m_alreadyRunning;
    }
};
#endif

int Run(int argc, wchar_t **argv)
{
    try {
        Configuration::SetDefaultConfiguration(defaultConfiguration);

        std::filesystem::path parentPath = std::filesystem::path(argv[0]).parent_path();
        if (parentPath.empty()) {
            parentPath = std::filesystem::current_path();
        }

#if _WIN32
        if (SetCurrentDirectoryW((LPWSTR)parentPath.wstring().c_str()) == FALSE) {
            std::cout << "GetLastError(): " << GetLastError() << ", with path: " << parentPath.string();

            MessageBoxA(NULL, "Failed to set directory!", "EstGame Error", MB_ICONERROR);
            return -1;
        }
#else
        if (chdir(parentPath.string().c_str()) != 0) {
            std::cout << "Failed to set directory!" << std::endl;
            return -1;
        }
#endif

        for (int i = 1; i < argc; i++) {
            std::wstring arg = argv[i];

            if (arg == L"--test-bms") {
                auto findPath = [&](const std::string &rel) -> std::filesystem::path {
                    std::filesystem::path p = rel;
                    if (std::filesystem::exists(p)) return p;
                    p = parentPath / rel;
                    if (std::filesystem::exists(p)) return p;
                    p = parentPath / ".." / ".." / ".." / rel;
                    if (std::filesystem::exists(p)) return p;
                    return rel;
                };

                std::filesystem::path p4k = findPath("1040.A Reyvateil_s Curse 4K/Helblinde - A Reyvateil_s Curse.bms");
                std::filesystem::path p7k = findPath("10000.Burst Linker 7K/Burst Linker [EX].bms");

                BMS::BMSFile bms4k;
                bms4k.Load(p4k);
                Chart chart4k(bms4k);

                BMS::BMSFile bms7k;
                bms7k.Load(p7k);
                Chart chart7k(bms7k);

                std::cout << "TEST_BMS_4K: KeyCount=" << bms4k.KeyCount << " ChartKeyCount=" << chart4k.m_keyCount << " Notes=" << bms4k.Notes.size() << std::endl;
                std::cout << "TEST_BMS_7K: KeyCount=" << bms7k.KeyCount << " ChartKeyCount=" << chart7k.m_keyCount << " Notes=" << bms7k.Notes.size() << std::endl;

                bool ok = (chart4k.m_keyCount == 4 && chart7k.m_keyCount == 7);
                std::cout << (ok ? "TEST_BMS_RESULT: SUCCESS" : "TEST_BMS_RESULT: FAILED") << std::endl;
                return ok ? 0 : 1;
            }

            // --autoplay, -a
            if (arg.find(L"--autoplay") != std::wstring::npos || arg.find(L"-a") != std::wstring::npos) {
                EnvironmentSetup::SetInt("ParameterAutoplay", 1);
            }

            // --rate, -r [float value range 0.5 - 2.0]
            if (arg.find(L"--rate") != std::wstring::npos || arg.find(L"-r") != std::wstring::npos) {
                if (i + 1 < argc) {
                    float rate = std::stof(argv[i + 1]);
                    rate = std::clamp(rate, 0.5f, 2.0f);

                    EnvironmentSetup::Set("ParameterRate", std::to_string(rate));
                }
            }

            if (std::filesystem::exists(argv[i]) && EnvironmentSetup::GetPath("FILE").empty()) {
                std::filesystem::path path = argv[i];
                EnvironmentSetup::SetInt("FileOpen", 1);
                EnvironmentSetup::SetPath("FILE", path);
            }
        }

#if _WIN32
        SingleInstanceLock singleInstance(L"Local\\O2Game_SingleInstance_Mutex");
#else
        SingleInstanceLock singleInstance("o2game_single_instance");
#endif
        if (singleInstance.IsAlreadyRunning()) {
            Logs::Write("Main", "Another instance of O2Game is already running. Exiting.");
            Logs::Flush();

#if _WIN32
            HWND existingWnd = FindWindowA("SDL_app", "O2Clone");
            if (!existingWnd) {
                existingWnd = FindWindowA("SDL_app", "Game");
            }
            if (!existingWnd) {
                existingWnd = FindWindowA(NULL, "O2Clone");
            }
            if (!existingWnd) {
                existingWnd = FindWindowA(NULL, "Game");
            }

            if (existingWnd) {
                if (IsIconic(existingWnd)) {
                    ShowWindow(existingWnd, SW_RESTORE);
                }
                SetForegroundWindow(existingWnd);
            }

            MessageBoxW(
                NULL,
                L"Another instance of O2Game is already running.",
                L"O2Game",
                MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND
            );
#else
            std::cout << "Another instance of O2Game is already running." << std::endl;
#endif
            return 0;
        }

        Logs::Write("Main", "Initializing game application...");
        Logs::Flush();

        MyGame game;
        if (!game.Init()) {
            Logs::Write("Main", "Game initialization failed. Please check game.log for details.");
            Logs::Flush();
#if _WIN32
            MessageBoxW(
                NULL,
                L"O2Game failed to start.\n\nPlease check 'game.log' in the game folder for error details.",
                L"Startup Error - O2Game",
                MB_OK | MB_ICONERROR | MB_TOPMOST | MB_SETFOREGROUND
            );
#endif
            return -1;
        }

        Logs::Write("Main", "Game initialized successfully. Entering main loop.");
        Logs::Flush();
        game.Run();
        Logs::Write("Main", "Game main loop ended cleanly.");
        Logs::Flush();

        return 0;
    } catch (const std::exception &e) {
        Logs::Write("Main", "Fatal std::exception caught: %s", e.what());
        Logs::Flush();
        CrashHandler::WriteCrashReport(std::string("Fatal C++ Exception: ") + e.what(), nullptr);
        return -1;
    } catch (...) {
        Logs::Write("Main", "Fatal unknown exception caught in main.");
        Logs::Flush();
        CrashHandler::WriteCrashReport("Fatal unknown exception caught in main", nullptr);
        return -1;
    }
}

#if _WIN32
int HandleStructualException(EXCEPTION_POINTERS *pExInfo)
{
    CrashHandler::WriteCrashReport("Unhandled Exception in main()", pExInfo);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char *argv[])
{
    CrashHandler::Install();

    const char *retVal = setlocale(LC_ALL, "en_US.UTF-8");
    if (retVal == nullptr) {
#if _WIN32
        MessageBoxA(NULL, "setlocale(): Failed to set locale!", "EstGame Error", MB_ICONERROR);
#else
        std::cout << "setlocale(): Failed to set locale!" << std::endl;
#endif
        CrashHandler::Uninstall();
        return -1;
    }

    wchar_t **wargv = new wchar_t *[argc];
    for (int i = 0; i < argc; i++) {
        size_t len = mbstowcs(NULL, argv[i], 0) + 1;
        wargv[i] = new wchar_t[len];
        mbstowcs(wargv[i], argv[i], len);
    }

    int ret = 0;

#if _WIN32 & _MSC_VER & NDEBUG
    __try {
        ret = Run(argc, wargv);
    }
    __except (HandleStructualException(GetExceptionInformation())) {
        ret = -1;
    }
#else
    ret = Run(argc, wargv);
#endif

    for (int i = 0; i < argc; i++) {
        delete[] wargv[i];
    }

    delete[] wargv;

    CrashHandler::Uninstall();
    return ret;
}

// #include "./Engine/LuaScripting.h"

// int main() {
// 	LuaScripting lua = { std::filesystem::current_path() / "Skins" / "Default" / "Scripts" };
// 	lua.Update(0.0);

// 	auto val = lua.GetSprite(SkinGroup::Playing, "JamLogo");
// 	return 0;
// }
