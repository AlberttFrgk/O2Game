/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "DefaultConfiguration.h"

#include <Misc/ini.h>
#include <filesystem>

using namespace Misc;

bool contains(mINI::INIStructure &ini, const std::string &section, const std::string &key)
{
    if (!ini.has(section)) {
        return false;
    }

    return ini[section].has(key);
}

void write(mINI::INIStructure &ini, const std::string &section, const std::string &key, const std::string &value)
{
    ini[section][key] = value;
}

void DefaultConfiguration::CreateDefault()
{
    auto path = std::filesystem::current_path() / "Game.ini";

    mINI::INIStructure ini;

    if (std::filesystem::exists(path)) {
        mINI::INIFile file(path.string());
        file.read(ini);
    }

    {
        if (!contains(ini, "Game", "skin")) {
            write(ini, "Game", "skin", "Default");
        }

        if (!contains(ini, "Game", "audiopitch")) {
            write(ini, "Game", "AudioPitch", "0");
        }

        if (!contains(ini, "Game", "framelimit")) {
            write(ini, "Game", "framelimit", "240");
        }

        if (!contains(ini, "Game", "audiooffset")) {
            write(ini, "Game", "audiooffset", "0");
        }

        if (!contains(ini, "Game", "audiovolume")) {
            write(ini, "Game", "audiovolume", "100");
        }

        if (!contains(ini, "Game", "autosound")) {
            write(ini, "Game", "autosound", "1");
        }

        if (!contains(ini, "Game", "guideline")) {
            write(ini, "Game", "guideline", "1");
        }

        if (!contains(ini, "Game", "notespeed")) {
            write(ini, "Game", "notespeed", "220");
        }
    }

    {
        if (!contains(ini, "Graphics", "width")) {
            write(ini, "Graphics", "width", "800");
        }

        if (!contains(ini, "Graphics", "height")) {
            write(ini, "Graphics", "height", "600");
        }

        if (!contains(ini, "Graphics", "renderer")) {
            write(ini, "Graphics", "renderer", "0");
        }

        if (!contains(ini, "Graphics", "renderthread")) {
            write(ini, "Graphics", "renderthread", "1");
        }

        if (!contains(ini, "Graphics", "vsync")) {
            write(ini, "Graphics", "vsync", "1");
        }

        if (!contains(ini, "Graphics", "fullscreen")) {
            write(ini, "Graphics", "fullscreen", "0");
        }

        if (!contains(ini, "Graphics", "framemode")) {
            write(ini, "Graphics", "framemode", "0");
        }

        if (!contains(ini, "Graphics", "framelimit")) {
            write(ini, "Graphics", "framelimit", "240");
        }
    }

    {
        if (!contains(ini, "keymapping", "lane1")) {
            write(ini, "keymapping", "lane1", "A");
        }

        if (!contains(ini, "keymapping", "lane2")) {
            write(ini, "keymapping", "lane2", "S");
        }

        if (!contains(ini, "keymapping", "lane3")) {
            write(ini, "keymapping", "lane3", "D");
        }

        if (!contains(ini, "keymapping", "lane4")) {
            write(ini, "keymapping", "lane4", "Space");
        }

        if (!contains(ini, "keymapping", "lane5")) {
            write(ini, "keymapping", "lane5", "J");
        }

        if (!contains(ini, "keymapping", "lane6")) {
            write(ini, "keymapping", "lane6", "K");
        }

        if (!contains(ini, "keymapping", "lane7")) {
            write(ini, "keymapping", "lane7", "L");
        }

        if (!contains(ini, "keymapping", "6_lane1")) {
            write(ini, "keymapping", "6_lane1", "A");
        }

        if (!contains(ini, "keymapping", "6_lane2")) {
            write(ini, "keymapping", "6_lane2", "S");
        }

        if (!contains(ini, "keymapping", "6_lane3")) {
            write(ini, "keymapping", "6_lane3", "F");
        }

        if (!contains(ini, "keymapping", "6_lane4")) {
            write(ini, "keymapping", "6_lane4", "J");
        }

        if (!contains(ini, "keymapping", "6_lane5")) {
            write(ini, "keymapping", "6_lane5", "K");
        }

        if (!contains(ini, "keymapping", "6_lane6")) {
            write(ini, "keymapping", "6_lane6", "L");
        }

        if (!contains(ini, "keymapping", "5_lane1")) {
            write(ini, "keymapping", "5_lane1", "A");
        }

        if (!contains(ini, "keymapping", "5_lane2")) {
            write(ini, "keymapping", "5_lane2", "S");
        }

        if (!contains(ini, "keymapping", "5_lane3")) {
            write(ini, "keymapping", "5_lane3", "Space");
        }

        if (!contains(ini, "keymapping", "5_lane4")) {
            write(ini, "keymapping", "5_lane4", "K");
        }

        if (!contains(ini, "keymapping", "5_lane5")) {
            write(ini, "keymapping", "5_lane5", "L");
        }

        if (!contains(ini, "keymapping", "4_lane1")) {
            write(ini, "keymapping", "4_lane1", "A");
        }

        if (!contains(ini, "keymapping", "4_lane2")) {
            write(ini, "keymapping", "4_lane2", "S");
        }

        if (!contains(ini, "keymapping", "4_lane3")) {
            write(ini, "keymapping", "4_lane3", "K");
        }

        if (!contains(ini, "keymapping", "4_lane4")) {
            write(ini, "keymapping", "4_lane4", "L");
        }

        if (!contains(ini, "keymapping", "notespeedup")) {
            write(ini, "keymapping", "notespeedup", "F4");
        }

        if (!contains(ini, "keymapping", "notespeeddown")) {
            write(ini, "keymapping", "notespeeddown", "F3");
        }
    }

    {
        if (!contains(ini, "music", "folder")) {
            std::filesystem::path currentDir = std::filesystem::current_path() / "Music";
            write(ini, "music", "folder", currentDir.string());
        }
    }

    mINI::INIFile file(path.string());
    file.write(ini, true);
}