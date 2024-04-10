#include "EnvironmentSetup.hpp"
#include <unordered_map>

namespace {
    std::unordered_map<std::string, std::string>           m_stores;
    std::unordered_map<std::string, void*>                m_storesPtr;
    std::unordered_map<std::string, std::filesystem::path> m_paths;
    std::unordered_map<std::string, int>                   m_ints;
} // namespace

void EnvironmentSetup::OnExitCheck()
{
    m_stores.clear();
    m_paths.clear();
    m_ints.clear();
}

void EnvironmentSetup::Set(std::string key, std::string value)
{
    m_stores[key] = std::move(value);
}

std::string EnvironmentSetup::Get(std::string key)
{
    auto it = m_stores.find(key);
    if (it != m_stores.end()) {
        return it->second;
    }
    return "";
}

void EnvironmentSetup::SetObj(std::string key, void* ptr)
{
    m_storesPtr[key] = ptr;
}

void* EnvironmentSetup::GetObj(std::string key)
{
    auto it = m_storesPtr.find(key);
    if (it != m_storesPtr.end()) {
        return it->second;
    }
    return nullptr;
}

void EnvironmentSetup::SetPath(std::string key, std::filesystem::path path)
{
    m_paths[key] = std::move(path);
}

std::filesystem::path EnvironmentSetup::GetPath(std::string key)
{
    auto it = m_paths.find(key);
    if (it != m_paths.end()) {
        return it->second;
    }
    return {};
}

void EnvironmentSetup::SetInt(std::string key, int value)
{
    m_ints[key] = value;
}

int EnvironmentSetup::GetInt(std::string key)
{
    auto it = m_ints.find(key);
    if (it != m_ints.end()) {
        return it->second;
    }
    return 0;
}