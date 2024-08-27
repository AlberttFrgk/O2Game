#define _CRTDBG_MAP_ALLOC
#undef NDEBUG

// STD Headers
#include <algorithm>
#include <filesystem>
#include <locale>
#include <windows.h>

// Game Headers
#include "./Data/Util/Util.hpp"
#include "./Resources/DefaultConfiguration.h"
#include "EnvironmentSetup.hpp"
#include "MyGame.h"

// Engine Headers
#include "Configuration.h"
#include "MsgBox.h"

// SDL Headers
#include <SDL.h>

#if _WIN32
extern "C" {
    __declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
    __declspec(dllexport) DWORD IntelGpuPowerPreference = 0x00000002;
}
#endif

int Run(int argc, wchar_t** argv)
{
    try {
        Configuration::SetDefaultConfiguration(defaultConfiguration);

        std::filesystem::path parentPath = std::filesystem::path(argv[0]).parent_path();
        if (parentPath.empty()) {
            parentPath = std::filesystem::current_path();
        }

#if _WIN32
        if (SetCurrentDirectoryW((LPWSTR)parentPath.wstring().c_str()) == FALSE) {
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

        MyGame game;
        if (game.Init()) {
            game.Run();
        }

        return 0;
    }
    catch (std::exception& e) {
        MsgBox::ShowOut("EstGame Error", e.what(), MsgBoxType::OK, MsgBoxFlags::BTN_ERROR);
        return -1;
    }
}

#if _WIN32
int HandleStructualException(int code)
{
    MessageBoxA(NULL, ("Uncaught exception: " + std::to_string(code)).c_str(), "FATAL ERROR", MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

// SDL expects an SDL_main function as the entry point
int SDL_main(int argc, char* argv[])
{
    const char* retVal = setlocale(LC_ALL, "en_US.UTF-8");
    if (retVal == nullptr) {
        MessageBoxA(NULL, "setlocale(): Failed to set locale!", "EstGame Error", MB_ICONERROR);
        return -1;
    }

    // Convert command line arguments
    int wargc = 0;
    wchar_t** wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);

    int ret = 0;

    __try {
        ret = Run(wargc, wargv);
    }
    __except (HandleStructualException(GetExceptionCode())) {
        ret = -1;
    }

    LocalFree(wargv);

    return ret;
}
#else
int main(int argc, char* argv[])
{
    const char* retVal = setlocale(LC_ALL, "en_US.UTF-8");
    if (retVal == nullptr) {
#if _WIN32
        MessageBoxA(NULL, "setlocale(): Failed to set locale!", "EstGame Error", MB_ICONERROR);
#else
        std::cout << "setlocale(): Failed to set locale!" << std::endl;
#endif
        return -1;
    }

    wchar_t** wargv = new wchar_t* [argc];
    for (int i = 0; i < argc; i++) {
        size_t len = mbstowcs(NULL, argv[i], 0) + 1;
        wargv[i] = new wchar_t[len];
        mbstowcs(wargv[i], argv[i], len);
    }

    int ret = 0;

#if _MSC_VER && !defined(NDEBUG)
    __try {
        ret = Run(argc, wargv);
    }
    __except (HandleStructualException(GetExceptionCode())) {
        ret = -1;
    }
#else
    ret = Run(argc, wargv);
#endif

    for (int i = 0; i < argc; i++) {
        delete[] wargv[i];
    }

    delete[] wargv;

    return ret;
}
#endif
