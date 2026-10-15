#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace EnvironmentSetup {
    void OnExitCheck();

    void        Set(std::string_view key, std::string value);
    std::string Get(std::string_view key);

    void  SetObj(std::string_view key, void *ptr);
    void *GetObj(std::string_view key);

    void SetInt(std::string_view key, int value);
    int  GetInt(std::string_view key);

    void                  SetPath(std::string_view key, std::filesystem::path path);
    std::filesystem::path GetPath(std::string_view key);
} // namespace EnvironmentSetup