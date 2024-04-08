/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#ifndef __LUA_MANAGER_H
#define __LUA_MANAGER_H

#include "LuaSkin.h"

class LuaManager
{
public:
    void                     LoadSkin(std::string skinName);
    std::shared_ptr<LuaSkin> LoadScript(SkinGroup group);

    std::string GetSkinProp(std::string group, std::string key, std::string defaultValue = "");
    std::string GetPath();

    static LuaManager *Get();

private:
    LuaManager() = default;
    ~LuaManager() = default;

    std::filesystem::path    CurrentPath;
    Misc::mINI::INIStructure ini;
};

#endif // __LUA_MANAGER_H