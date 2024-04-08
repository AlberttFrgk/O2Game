/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "LuaManager.h"
#include <Exceptions/EstException.h>

void LuaManager::LoadSkin(std::string skinName)
{
#if 1
    CurrentPath = std::filesystem::current_path() / ".." / ".." / "resources" / "Resources";
#else
    CurrentPath = std::filesystem::current_path() / "Skins" / skinName;
#endif
    if (!std::filesystem::exists(CurrentPath)) {
        throw Exceptions::EstException("Skin %s does not exist", skinName.c_str());
    }

    auto path = CurrentPath / "GameSkin.ini";
    if (!std::filesystem::exists(path)) {
        throw Exceptions::EstException("GameSkin.ini does not exist");
    }

    Misc::mINI::INIFile file(path.string());
    file.read(ini);
}

std::shared_ptr<LuaSkin> LuaManager::LoadScript(SkinGroup group)
{
    return std::make_shared<LuaSkin>(CurrentPath, group);
}

std::string LuaManager::GetSkinProp(std::string group, std::string key, std::string defaultValue)
{
    return ini[group][key].empty() ? defaultValue : ini[group][key];
}

std::string LuaManager::GetPath()
{
    return CurrentPath.string();
}

LuaManager *LuaManager::Get()
{
    static LuaManager instance;
    return &instance;
}