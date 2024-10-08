#pragma once
#include <Misc/mINI.h>
#include <filesystem>
#include <iostream>
#include <map>
#include <vector>

#include "LuaScripting.h"
#include "SkinConfig.hpp"

class SkinManager
{
public:
    void LoadSkin(std::string skinName);
    void ReloadSkin();

    LaneInfo GetLaneInfo();

    std::string           GetSkinProp(std::string group, std::string key, std::string defaultValue = "");
    std::filesystem::path GetPath();

    void                       SetKeyCount(int key);
    std::vector<NumericValue>  GetNumeric(SkinGroup group, std::string key);
    std::vector<PositionValue> GetPosition(SkinGroup group, std::string key);
    std::vector<RectInfo>      GetRect(SkinGroup group, std::string key);
    NoteValue                  GetNote(SkinGroup group, std::string key);
    SpriteValue                GetSprite(SkinGroup group, std::string key);

    void                       Arena_SetIndex(int index);
    std::vector<NumericValue>  Arena_GetNumeric(std::string key);
    std::vector<PositionValue> Arena_GetPosition(std::string key);
    std::vector<RectInfo>      Arena_GetRect(std::string key);
    SpriteValue                Arena_GetSprite(std::string key);

    void Update(double delta);

    static SkinManager *GetInstance();
    static void         Release();

private:
    SkinManager();
    ~SkinManager();

    void TryLoadGroup(SkinGroup group);

    static SkinManager *m_instance;

    std::unique_ptr<SkinConfig>                      m_skinConfig;
    std::unique_ptr<LuaScripting>                    m_luaScripting;
    std::map<SkinGroup, std::unique_ptr<SkinConfig>> m_skinConfigs;
    std::map<SkinGroup, std::string>                 m_expected_directory;
    std::map<SkinGroup, std::string>                 m_expected_skin_config;

    std::string                 m_currentSkin;
    mINI::INIStructure          ini;
    std::unique_ptr<SkinConfig> m_arenaConfig;

    int  m_keyCount, m_previousKeyCount;
    int  m_arena;
    bool m_useLua;
};