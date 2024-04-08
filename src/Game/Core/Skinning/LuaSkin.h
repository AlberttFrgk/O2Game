/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#pragma once

#include "SkinStructs.h"
#include <Misc/ini.h>
#include <filesystem>
#include <sol/sol.hpp>
#include <tuple>
#include <vector>

class LuaSkin;
struct GameLua
{
    int                  GetArenaIndex();
    int                  GetHitPosition();
    int                  GetLaneOffset();
    std::tuple<int, int> GetResolution();
    int                  GetKeyCount();
    std::string          GetSkinPath();
    std::string          GetFontPath();
    std::string          GetScriptPath();

    /* Utility */

    bool                IsPathExist(std::string Path);
    std::pair<int, int> GetImageSize(std::string Path);

    /* Font ranges */
    sol::table GetCharRange(CharRangeType type);

    LuaSkin    *__skin;
    std::string __group;
};

struct LuaState
{
    std::shared_ptr<sol::state> state;
    std::shared_ptr<sol::table> table;
    std::shared_ptr<GameLua>    game;
};

class LuaSkin
{
public:
    LuaSkin() = default;
    LuaSkin(std::filesystem::path path, SkinGroup group);
    ~LuaSkin();

    std::vector<NumericValue>  GetNumeric(std::string key);
    std::vector<PositionValue> GetPosition(std::string key);
    std::vector<RectInfo>      GetRect(std::string key);
    std::vector<AudioInfo>     GetAudio();
    std::vector<FontInfo>      GetFont(std::string key);
    SpriteValue                GetSprite(std::string key);
    TweenInfo                  GetTween(std::string key);
    AnimationInfo              GetAnimation(std::string key);

    LuaState *GetState();

private:
    SkinGroup CurrentGroup;
    LuaState  Script;
};