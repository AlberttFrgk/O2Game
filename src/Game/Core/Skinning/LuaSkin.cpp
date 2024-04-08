/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "../../Env.h"
#define SOL_ALL_SAFETIES_ON 1
#include "LuaSkin.h"
#include "LuaManager.h"
#include <Graphics/Utils/stb_image.h>

#include <Exceptions/EstException.h>
#include <Graphics/NativeWindow.h>
#include <map>

// Bindings
#include "Bindings/LuaMath.h"
#include "Bindings/LuaColor3.h"
#include "Bindings/LuaUDim.h"
#include "Bindings/LuaUDim2.h"
#include "Bindings/LuaVector2.h"

// For the char range
#include <Imgui/imgui.h>

static std::map<SkinGroup, std::string> ExpectedFiles = {
    { SkinGroup::Main, "Main.lua" },
    { SkinGroup::MainMenu, "MainMenu.lua" },
    { SkinGroup::Notes, "Notes.lua" },
    { SkinGroup::Playing, "Playing.lua" },
    { SkinGroup::Arena, "Arena.lua" },
    { SkinGroup::SongSelect, "SongSelect.lua" },
    { SkinGroup::Result, "Result.lua" },
    { SkinGroup::Audio, "Audio.lua" }
};

static std::map<SkinGroup, std::string> ExpectedGroups = {
    { SkinGroup::Main, "Main" },
    { SkinGroup::MainMenu, "MainMenu" },
    { SkinGroup::Notes, "Notes" },
    { SkinGroup::Playing, "Playing" },
    { SkinGroup::Arena, "Arena" },
    { SkinGroup::SongSelect, "SongSelect" },
    { SkinGroup::Result, "Result" },
    { SkinGroup::Audio, "Audio" }
};

inline void panic_handler(sol::optional<std::string> maybe_msg)
{
    if (maybe_msg) {
        throw Exceptions::EstException("LuaSkin fail: %s", maybe_msg.value().c_str());
    } else {
        throw Exceptions::EstException("An unexpected error occurred and panic has been invoked");
    }
}

LuaSkin::LuaSkin(std::filesystem::path _path, SkinGroup group)
{
    auto file = ExpectedFiles[group];
    auto path = _path / "Scripts" / file;
    if (!std::filesystem::exists(path)) {
        throw Exceptions::EstException("Script %s does not exist", ExpectedFiles[group].c_str());
    }

    CurrentGroup = group;

    Script.table.reset();
    Script.state = std::make_shared<sol::state>();
    sol::state &state = *Script.state.get();

    state.open_libraries(
        sol::lib::base,
        sol::lib::string,
        sol::lib::table,
        sol::lib::io,
        sol::lib::package);

    sol::table HeaderType = state.create_table();
    HeaderType["Playing"] = SkinGroup::Playing;
    HeaderType["SongSelect"] = SkinGroup::SongSelect;
    HeaderType["MainMenu"] = SkinGroup::MainMenu;
    HeaderType["Arena"] = SkinGroup::Arena;
    HeaderType["Notes"] = SkinGroup::Notes;
    HeaderType["Audio"] = SkinGroup::Audio;

    sol::table DataType = state.create_table();
    DataType["Numeric"] = SkinDataType::Numeric;
    DataType["Position"] = SkinDataType::Position;
    DataType["Rect"] = SkinDataType::Rect;
    DataType["Note"] = SkinDataType::Note;
    DataType["Sprite"] = SkinDataType::Sprite;
    DataType["Tween"] = SkinDataType::Tween;
    DataType["Audio"] = SkinDataType::Audio;
    DataType["Font"] = SkinDataType::Font;
    DataType["Animation"] = SkinDataType::Animation;

    sol::table TweenType = state.create_table();
    TweenType["Linear"] = TweenType::Linear;
    TweenType["Quadratic"] = TweenType::Quadratic;
    TweenType["Cubic"] = TweenType::Cubic;
    TweenType["Quartic"] = TweenType::Quartic;
    TweenType["Quintic"] = TweenType::Quintic;
    TweenType["Sinusoidal"] = TweenType::Sinusoidal;
    TweenType["Exponential"] = TweenType::Exponential;
    TweenType["Circular"] = TweenType::Circular;
    TweenType["Elastic"] = TweenType::Elastic;
    TweenType["Back"] = TweenType::Back;
    TweenType["Bounce"] = TweenType::Bounce;

    sol::table NumericDirection = state.create_table();
    NumericDirection["Left"] = SkinNumericDirection::Left;
    NumericDirection["Mid"] = SkinNumericDirection::Mid;
    NumericDirection["Right"] = SkinNumericDirection::Right;

    sol::table BGMType = state.create_table();
    BGMType["Lobby"] = SkinAudioType::BGM_Lobby;
    BGMType["Waiting"] = SkinAudioType::BGM_Waiting;
    BGMType["Result"] = SkinAudioType::BGM_Result;

    sol::table CharRange = state.create_table();
    CharRange["Default"] = CharRangeType::Default;
    CharRange["Japanese"] = CharRangeType::Japanese;
    CharRange["Korean"] = CharRangeType::Korean;
    CharRange["Chinese"] = CharRangeType::Chinese;

    sol::table Enums = state.create_table();
    Enums["HeaderType"] = HeaderType;
    Enums["DataType"] = DataType;
    Enums["TweenType"] = TweenType;
    Enums["NumericDirection"] = NumericDirection;
    Enums["AudioType"] = BGMType;
    Enums["CharRange"] = CharRange;

    state["Enum"] = Enums;

    state["LerpEasing"] = [&](int easingType, UDim2 start, UDim2 end, double t) -> UDim2 {
        ::TweenType type = static_cast<::TweenType>(easingType);

        return Tween::Lerp(type, start, end, static_cast<float>(t));
    };

    LuaMath::Register(state);
    LuaColor3::Register(state);
    LuaUDim::Register(state);
    LuaUDim2::Register(state);
    LuaVector2::Register(state);

    state.new_usertype<GameLua>(
        "GameLua",
        "new", sol::no_constructor,
        "GetArenaIndex", &GameLua::GetArenaIndex,
        "GetHitPosition", &GameLua::GetHitPosition,
        "GetLaneOffset", &GameLua::GetLaneOffset,
        "GetResolution", &GameLua::GetResolution,
        "GetKeyCount", &GameLua::GetKeyCount,
        "GetSkinPath", &GameLua::GetSkinPath,
        "GetScriptPath", &GameLua::GetScriptPath,
        "IsPathExist", &GameLua::IsPathExist,
        "GetImageSize", &GameLua::GetImageSize,
        "GetCharRange", &GameLua::GetCharRange,
        "GetFontPath", &GameLua::GetFontPath);

    Script.game = std::make_shared<GameLua>();
    Script.game->__skin = this;
    Script.game->__group = ExpectedGroups[group];
    state["Game"] = Script.game.get();

    try {
        sol::table table = state.script_file(
            path.string(),
            sol::load_mode::text);

        Script.table = std::make_shared<sol::table>(std::move(table));

    } catch (const sol::error &e) {
        throw Exceptions::EstException("Failed to load script [%s]: %s", file.c_str(), e.what());
    }
}

const char *get_type_name(sol::type type)
{
    switch (type) {
        case sol::type::none:
            return "none";
        case sol::type::nil:
            return "nil";
        case sol::type::boolean:
            return "boolean";
        case sol::type::number:
            return "number";
        case sol::type::string:
            return "string";
        case sol::type::table:
            return "table";
        case sol::type::function:
            return "function";
        case sol::type::userdata:
            return "userdata";
        case sol::type::thread:
            return "thread";
        case sol::type::lightuserdata:
            return "lightuserdata";
        case sol::type::poly:
            return "poly";
        default:
            return "unknown";
    }
}

std::vector<NumericValue> LuaSkin::GetNumeric(std::string key)
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Numeric][key] == sol::nil || !table[SkinDataType::Numeric][key].is<sol::table>()) {
            throw Exceptions::EstException("[Numeric] [%s] Key '%s' does not exist or not table", luaFileName.c_str(), key.c_str());
        }

        sol::table                key_array = table[SkinDataType::Numeric][key];
        std::vector<NumericValue> result;
        for (auto &value : key_array) {
            if (!value.second.is<sol::table>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', Value is not a table but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value.second.get_type()));
            }

            NumericValue numeric_value = {};
            sol::table   value_table = value.second;

            if (value_table["Path"] == sol::nil || !value_table["Path"].is<std::string>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', Path field is not a string or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Path"].get_type()));
            }

            std::string path = value_table["Path"];
            numeric_value.Path = path;

            if (value_table["TexCoords"] == sol::nil) {
                auto skinSize = Script.game->GetImageSize(path);

                int width = skinSize.first;
                int height = skinSize.second;

                std::vector<std::vector<glm::vec2>> texCoords = {
                    { { 0, 0 }, { width, 0 }, { 0, height }, { width, height } }
                };

                numeric_value.TexCoords = texCoords;
            } else {
                if (!value_table["TexCoords"].is<sol::table>()) {
                    throw Exceptions::EstException(
                        "[Numeric] %s at key: '%s', TexCoords field is not a table or nil but got %s",
                        luaFileName.c_str(),
                        key.c_str(),
                        get_type_name(value_table[1].get_type()));
                }

                sol::table values = value_table["TexCoords"];
                for (auto &value : values) {
                    if (!value.second.is<sol::table>()) {
                        throw Exceptions::EstException(
                            "[Numeric] %s at key: '%s', TexCoords value is not a table but got %s",
                            luaFileName.c_str(),
                            key.c_str(),
                            get_type_name(value.second.get_type()));
                    }

                    sol::table             itr = value.second;
                    std::vector<glm::vec2> texCoords;

                    for (auto &itr_value : itr) {
                        if (!itr_value.second.is<sol::table>()) {
                            throw Exceptions::EstException(
                                "[Numeric] %s at key: '%s', TexCoords value is not a table but got %s",
                                luaFileName.c_str(),
                                key.c_str(),
                                get_type_name(itr_value.second.get_type()));
                        }

                        sol::table itr_table = itr_value.second;
                        if (itr_table[1] == sol::nil || !itr_table[1].is<double>()) {
                            throw Exceptions::EstException("[Numeric] %s at key: '%s', TexCoords[1] is not a number or nil", luaFileName.c_str(), key.c_str());
                        }

                        if (itr_table[2] == sol::nil || !itr_table[2].is<double>()) {
                            throw Exceptions::EstException("[Numeric] %s at key: '%s', TexCoords[2] is not a number or nil", luaFileName.c_str(), key.c_str());
                        }

                        double x = itr_table[1];
                        double y = itr_table[2];

                        texCoords.push_back(glm::vec2(x, y));
                    }

                    numeric_value.TexCoords.push_back(texCoords);
                }
            }

            if (value_table["Position"] == sol::nil || !value_table["Position"].is<UDim2>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', Position field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Position"].get_type()));
            }

            numeric_value.Position = value_table["Position"];

            if (value_table["Size"] == sol::nil || !value_table["Size"].is<UDim2>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', Size field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Size"].get_type()));
            }

            numeric_value.Size = value_table["Size"];

            if (value_table["MaxDigit"] == sol::nil || !value_table["MaxDigit"].is<int>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', MaxDigit field is not a number or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["MaxDigit"].get_type()));
            }

            numeric_value.MaxDigit = value_table["MaxDigit"];

            if (value_table["Direction"] == sol::nil || !value_table["Direction"].is<int>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', Direction field is not a number or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Direction"].get_type()));
            }

            numeric_value.Direction = value_table["Direction"];

            if (value_table["FillWithZero"] == sol::nil || !value_table["FillWithZero"].is<bool>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', FillWithZero field is not a boolean or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["FillWithZero"].get_type()));
            }

            numeric_value.FillWithZero = value_table["FillWithZero"];

            if (value_table["Color"] == sol::nil || !value_table["Color"].is<Color3>()) {
                throw Exceptions::EstException(
                    "[Numeric] %s at key: '%s', Color field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Color"].get_type()));
            }

            numeric_value.Color = value_table["Color"];

            result.push_back(numeric_value);
        }

        if (result.empty()) {
            throw Exceptions::EstException("[Numeric] %s at key: '%s', NumericValue is empty", luaFileName.c_str(), key.c_str());
        }

        return result;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Numeric] %s, (key = %s)", err.what(), key.c_str());
    }
}

std::vector<PositionValue> LuaSkin::GetPosition(std::string key)
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Position][key] == sol::nil || !table[SkinDataType::Position][key].is<sol::table>()) {
            throw Exceptions::EstException("[Position] [%s] Key '%s' does not exist or not table", luaFileName.c_str(), key.c_str());
        }

        sol::table key_array = table[SkinDataType::Position][key];

        std::vector<PositionValue> result;
        for (auto &data : key_array) {
            if (!data.second.is<sol::table>()) {
                throw Exceptions::EstException(
                    "[Position] %s at key: '%s', Value is not a table but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(data.second.get_type()));
            }

            PositionValue position_value = {};
            sol::table    value_table = data.second;

            if (value_table["Path"] == sol::nil || !value_table["Path"].is<std::string>()) {
                throw Exceptions::EstException(
                    "[Position] %s at key: '%s', Path field is not a string or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Path"].get_type()));
            }

            std::string path = value_table["Path"];
            position_value.Path = path;

            if (value_table["TexCoord"] == sol::nil) {
                try {
                    auto skinSize = Script.game->GetImageSize(path);

                    int width = skinSize.first;
                    int height = skinSize.second;

                    std::vector<glm::vec2> texCoord = {
                        { 0, 0 }, { width, 0 }, { 0, height }, { width, height }
                    };

                    position_value.TexCoord = texCoord;
                } catch (const Exceptions::EstException &) {
                    std::vector<glm::vec2> texCoord = {
                        { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }
                    };

                    position_value.TexCoord = texCoord;
                }

            } else {
                if (!value_table["TexCoord"].is<sol::table>()) {
                    throw Exceptions::EstException(
                        "[Position] %s at key: '%s', TexCoord field is not a table or nil but got %s",
                        luaFileName.c_str(),
                        key.c_str(),
                        get_type_name(value_table["TexCoord"].get_type()));
                }

                sol::table             values = value_table["TexCoord"];
                std::vector<glm::vec2> texCoord;

                for (auto &value : values) {
                    if (!value.second.is<sol::table>()) {
                        throw Exceptions::EstException(
                            "[Position] %s at key: '%s', TexCoord value is not a table but got %s",
                            luaFileName.c_str(),
                            key.c_str(),
                            get_type_name(value.second.get_type()));
                    }

                    sol::table itr_table = value.second;
                    if (itr_table[1] == sol::nil || !itr_table[1].is<double>()) {
                        throw Exceptions::EstException("[Position] %s at key: '%s', TexCoord[1] is not a number or nil", luaFileName.c_str(), key.c_str());
                    }

                    if (itr_table[2] == sol::nil || !itr_table[2].is<double>()) {
                        throw Exceptions::EstException("[Position] %s at key: '%s', TexCoord[2] is not a number or nil", luaFileName.c_str(), key.c_str());
                    }

                    double x = itr_table[1];
                    double y = itr_table[2];

                    texCoord.push_back(glm::vec2(x, y));
                }

                position_value.TexCoord = texCoord;
            }

            if (value_table["Position"] == sol::nil || !value_table["Position"].is<UDim2>()) {
                throw Exceptions::EstException(
                    "[Position] %s at key: '%s', Position field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Position"].get_type()));
            }

            position_value.Position = value_table["Position"];

            if (value_table["Size"] == sol::nil || !value_table["Size"].is<UDim2>()) {
                throw Exceptions::EstException(
                    "[Position] %s at key: '%s', Size field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Size"].get_type()));
            }

            position_value.Size = value_table["Size"];

            if (value_table["AnchorPoint"] == sol::nil || !value_table["AnchorPoint"].is<Vector2>()) {
                throw Exceptions::EstException(
                    "[Position] %s at key: '%s', AnchorPoint field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["AnchorPoint"].get_type()));
            }

            position_value.AnchorPoint = value_table["AnchorPoint"];

            if (value_table["Color"] == sol::nil || !value_table["Color"].is<Color3>()) {
                throw Exceptions::EstException(
                    "[Position] %s at key: '%s', Color field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Color"].get_type()));
            }

            position_value.Color = value_table["Color"];

            result.push_back(position_value);
        }

        return result;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Position] %s, (key = %s)", err.what(), key.c_str());
    }
}

std::vector<RectInfo> LuaSkin::GetRect(std::string key)
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Rect][key] == sol::nil || !table[SkinDataType::Rect][key].is<sol::table>()) {
            throw Exceptions::EstException("[Rect] [%s] Key '%s' does not exist or not table", luaFileName.c_str(), key.c_str());
        }

        sol::table key_array = table[SkinDataType::Rect][key];

        std::vector<RectInfo> result;
        for (auto &value : key_array) {
            sol::table value_table = value.second;

            if (!value_table.is<sol::table>()) {
                throw Exceptions::EstException(
                    "[Rect] %s at key: '%s', Value is not a table but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table.get_type()));
            }

            RectInfo rect_info = {};

            if (value_table["Position"] == sol::nil || !value_table["Position"].is<UDim2>()) {
                throw Exceptions::EstException(
                    "[Rect] %s at key: '%s', Position field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Position"].get_type()));
            }

            rect_info.Position = value_table["Position"];

            if (value_table["Size"] == sol::nil || !value_table["Size"].is<UDim2>()) {
                throw Exceptions::EstException(
                    "[Rect] %s at key: '%s', Size field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Size"].get_type()));
            }

            rect_info.Size = value_table["Size"];

            result.push_back(rect_info);
        }

        if (result.empty()) {
            throw Exceptions::EstException("[Rect] %s at key: '%s', RectInfo is empty", luaFileName.c_str(), key.c_str());
        }

        return result;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Rect] %s, (key = %s)", err.what(), key.c_str());
    }
}

std::vector<AudioInfo> LuaSkin::GetAudio()
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Audio] == sol::nil || !table[SkinDataType::Audio].is<sol::table>()) {
            throw Exceptions::EstException("[Audio] [%s] Key 'Audio' does not exist or not table", luaFileName.c_str());
        }

        sol::table key_array = table[SkinDataType::Audio];

        std::vector<AudioInfo> result;
        for (auto &value : key_array) {
            if (!value.second.is<sol::table>()) {
                throw Exceptions::EstException(
                    "[Audio] %s at key: 'Audio', Value is not a table but got %s",
                    luaFileName.c_str(),
                    get_type_name(value.second.get_type()));
            }

            AudioInfo audio_info = {};

            sol::table value_table = value.second;

            if (value_table["Path"] == sol::nil || !value_table["Path"].is<std::string>()) {
                throw Exceptions::EstException(
                    "[Audio] %s at key: Audio', Path field is not a string or nil but got %s",
                    luaFileName.c_str(),
                    get_type_name(value_table["Path"].get_type()));
            }

            std::string path = value_table["Path"];
            audio_info.Path = path;

            if (value_table["Type"] == sol::nil || !value_table["Type"].is<int>()) {
                throw Exceptions::EstException(
                    "[Audio] %s at key: Audio', Type field is not a number or nil but got %s",
                    luaFileName.c_str(),
                    get_type_name(value_table["Type"].get_type()));
            }

            audio_info.Type = value_table["Type"];

            result.push_back(audio_info);
        }

        if (result.empty()) {
            throw Exceptions::EstException("[Audio] %s at key: 'Audio', AudioInfo is empty", luaFileName.c_str());
        }

        return result;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Audio] %s, (key = Audio)", err.what());
    }
}

SpriteValue LuaSkin::GetSprite(std::string key)
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Sprite][key] == sol::nil || !table[SkinDataType::Sprite][key].is<sol::table>()) {
            throw Exceptions::EstException("[Sprite] Key %s does not exist", key.c_str());
        }

        sol::table key_array = table[SkinDataType::Sprite][key];

        if (key_array["Path"] == sol::nil || !key_array["Path"].is<std::string>()) {
            throw Exceptions::EstException(
                "[Sprite] %s at key: '%s', Path field is not a string or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Path"].get_type()));
        }

        SpriteValue sprite_value = {};

        std::string path = key_array["Path"];
        sprite_value.Path = path;

        if (key_array["TexCoords"] == sol::nil) {
            auto size = Script.game->GetImageSize(path);

            int width = size.first;
            int height = size.second;

            std::vector<glm::vec2> texCoords = {
                { 0, 0 }, { width, 0 }, { 0, height }, { width, height }
            };

            sprite_value.TexCoords.push_back(texCoords);
        } else {
            if (!key_array["TexCoords"].is<sol::table>()) {
                throw Exceptions::EstException(
                    "[Sprite] %s at key: '%s', TexCoords field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(key_array["TexCoords"].get_type()));
            }

            sol::table values = key_array["TexCoords"];
            for (auto &value : values) {
                if (!value.second.is<sol::table>()) {
                    throw Exceptions::EstException(
                        "[Sprite] %s at key: '%s', TexCoord value is not a table but got %s",
                        luaFileName.c_str(),
                        key.c_str(),
                        get_type_name(value.second.get_type()));
                }

                sol::table             itr = value.second;
                std::vector<glm::vec2> texCoords;

                for (auto &itr_value : itr) {
                    if (!itr_value.second.is<sol::table>()) {
                        throw Exceptions::EstException(
                            "[Sprite] %s at key: '%s', TexCoord value is not a table but got %s",
                            luaFileName.c_str(),
                            key.c_str(),
                            get_type_name(itr_value.second.get_type()));
                    }

                    sol::table itr_table = itr_value.second;
                    if (itr_table[1] == sol::nil || !itr_table[1].is<double>()) {
                        throw Exceptions::EstException("[Sprite] %s at key: '%s', TexCoord[1] is not a number or nil", luaFileName.c_str(), key.c_str());
                    }

                    if (itr_table[2] == sol::nil || !itr_table[2].is<double>()) {
                        throw Exceptions::EstException("[Sprite] %s at key: '%s', TexCoord[2] is not a number or nil", luaFileName.c_str(), key.c_str());
                    }

                    double x = itr_table[1];
                    double y = itr_table[2];

                    texCoords.push_back(glm::vec2(x, y));
                }

                sprite_value.TexCoords.push_back(texCoords);
            }
        }

        if (key_array["Position"] == sol::nil || !key_array["Position"].is<UDim2>()) {
            throw Exceptions::EstException(
                "[Sprite] %s at key: '%s', Position field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Position"].get_type()));
        }

        sprite_value.Position = key_array["Position"];

        if (key_array["Size"] == sol::nil || !key_array["Size"].is<UDim2>()) {
            throw Exceptions::EstException(
                "[Sprite] %s at key: '%s', Size field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Size"].get_type()));
        }

        sprite_value.Size = key_array["Size"];

        if (key_array["AnchorPoint"] == sol::nil || !key_array["AnchorPoint"].is<Vector2>()) {
            throw Exceptions::EstException(
                "[Sprite] %s at key: '%s', AnchorPoint field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["AnchorPoint"].get_type()));
        }

        sprite_value.AnchorPoint = key_array["AnchorPoint"];

        if (key_array["FrameTime"] == sol::nil || !key_array["FrameTime"].is<double>()) {
            throw Exceptions::EstException(
                "[Sprite] %s at key: '%s', FrameTime field is not a number or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["FrameTime"].get_type()));
        }

        sprite_value.FrameTime = key_array["FrameTime"];

        if (key_array["Color"] == sol::nil || !key_array["Color"].is<Color3>()) {
            throw Exceptions::EstException(
                "[Sprite] %s at key: '%s', Color field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Color"].get_type()));
        }

        sprite_value.Color = key_array["Color"];

        return sprite_value;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Sprite] %s, (key = %s)", err.what(), key.c_str());
    }
}

TweenInfo LuaSkin::GetTween(std::string key)
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Tween][key] == sol::nil || !table[SkinDataType::Tween][key].is<sol::table>()) {
            throw Exceptions::EstException("[Tween] [%s] Key '%s' does not exist or not table", luaFileName.c_str(), key.c_str());
        }

        sol::table key_array = table[SkinDataType::Tween][key];

        TweenInfo tween_info = {};

        if (key_array["Destination"] == sol::nil || !key_array["Destination"].is<UDim2>()) {
            throw Exceptions::EstException(
                "[Tween] %s at key: '%s', Destination field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Destination"].get_type()));
        }

        tween_info.Destination = key_array["Destination"];

        if (key_array["Type"] == sol::nil || !key_array["Type"].is<int>()) {
            throw Exceptions::EstException(
                "[Tween] %s at key: '%s', Type field is not a number or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Type"].get_type()));
        }

        tween_info.Type = key_array["Type"];

        if (key_array["Duration"] == sol::nil || !key_array["Duration"].is<double>()) {
            throw Exceptions::EstException(
                "[Tween] %s at key: '%s', Duration field is not a number or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Duration"].get_type()));
        }

        tween_info.Duration = key_array["Duration"];

        return tween_info;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Tween] %s, (key = %s)", err.what(), key.c_str());
    }
}

std::vector<FontInfo> LuaSkin::GetFont(std::string key)
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Font][key] == sol::nil || !table[SkinDataType::Font][key].is<sol::table>()) {
            throw Exceptions::EstException("[Rect] [%s] Key '%s' does not exist or not table", luaFileName.c_str(), key.c_str());
        }

        sol::table key_array = table[SkinDataType::Font][key];

        std::vector<FontInfo> result;
        for (auto &value : key_array) {
            if (!value.second.is<sol::table>()) {
                throw Exceptions::EstException(
                    "[Font] %s at key: '%s', Value is not a table but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value.second.get_type()));
            }

            FontInfo font_info = {};

            sol::table value_table = value.second;
            if (value_table["FontFile"] == sol::nil || !value_table["FontFile"].is<std::string>()) {
                throw Exceptions::EstException(
                    "[Font] %s at key: '%s', FontFile field is not a string or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["FontFile"].get_type()));
            }

            std::string fontFile = value_table["FontFile"];
            font_info.FontFile = fontFile;

            if (!std::filesystem::exists(fontFile)) {
                throw Exceptions::EstException("[Font] %s at key: '%s', The font file is not exist! [File: %s]", luaFileName.c_str(), key.c_str(), fontFile.c_str());
            }

            if (value_table["Size"] == sol::nil || !value_table["Size"].is<double>()) {
                throw Exceptions::EstException(
                    "[Font] %s at key: '%s', Size field is not a number or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["Size"].get_type()));
            }

            double size = value_table["Size"];
            font_info.Size = static_cast<float>(size);

            if (value_table["CharRange"] == sol::nil || !value_table["CharRange"].is<sol::table>()) {
                throw Exceptions::EstException(
                    "[Font] %s at key: '%s', CharRange field is not a table or nil but got %s",
                    luaFileName.c_str(),
                    key.c_str(),
                    get_type_name(value_table["CharRange"].get_type()));
            }

            sol::table charRanges = value_table["CharRange"];
            for (auto &charRange : charRanges) {
                if (!charRange.second.is<sol::table>()) {
                    throw Exceptions::EstException(
                        "[Font] %s at key: '%s', CharRange value is not a table but got %s",
                        luaFileName.c_str(),
                        key.c_str(),
                        get_type_name(charRange.second.get_type()));
                }

                sol::table charRangeTable = charRange.second;
                if (charRangeTable[1] == sol::nil || !charRangeTable[1].is<int>()) {
                    throw Exceptions::EstException("[Font] %s at key: '%s', CharRange[1] is not a number or nil", luaFileName.c_str(), key.c_str());
                }

                if (charRangeTable[2] == sol::nil || !charRangeTable[2].is<int>()) {
                    throw Exceptions::EstException("[Font] %s at key: '%s', CharRange[2] is not a number or nil", luaFileName.c_str(), key.c_str());
                }

                font_info.CharRanges.push_back({ charRangeTable[1],
                                                 charRangeTable[2] });
            }

            result.push_back(font_info);
        }

        if (result.empty()) {
            throw Exceptions::EstException("[Font] %s at key: '%s', FontInfo is empty", luaFileName.c_str(), key.c_str());
        }

        return result;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Font] %s, (key = %s)", err.what(), key.c_str());
    }
}

AnimationInfo LuaSkin::GetAnimation(std::string key)
{
    try {
        sol::table &script_table = *Script.table.get();
        sol::table  table = script_table["Data"];
        auto        luaFileName = ExpectedFiles[CurrentGroup];

        // check if the key exists
        if (table[SkinDataType::Animation][key] == sol::nil || !table[SkinDataType::Animation][key].is<sol::table>()) {
            throw Exceptions::EstException("[Animation] [%s] Key '%s' does not exist or not table", luaFileName.c_str(), key.c_str());
        }

        sol::table key_array = table[SkinDataType::Animation][key];

        AnimationInfo animation_info = {};

        if (key_array["Position"] == sol::nil || !key_array["Position"].is<sol::table>()) {
            throw Exceptions::EstException(
                "[Animation] %s at key: '%s', Position field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Position"].get_type()));
        }

        sol::table position = key_array["Position"];

        if (position["Start"] == sol::nil || !position["Start"].is<UDim2>()) {
            throw Exceptions::EstException(
                "[Animation] %s at key: '%s', Position.Start field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(position["Start"].get_type()));
        }

        if (position["End"] == sol::nil || !position["End"].is<UDim2>()) {
            throw Exceptions::EstException(
                "[Animation] %s at key: '%s', Position.End field is not a table or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(position["End"].get_type()));
        }

        animation_info.Position.Start = position["Start"];
        animation_info.Position.End = position["End"];

        if (key_array["Duration"] == sol::nil || !key_array["Duration"].is<double>()) {
            throw Exceptions::EstException(
                "[Animation] %s at key: '%s', Duration field is not a number or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Duration"].get_type()));
        }

        animation_info.Duration = key_array["Duration"];

        if (key_array["Repeat"] == sol::nil || !key_array["Repeat"].is<bool>()) {
            throw Exceptions::EstException(
                "[Animation] %s at key: '%s', Repeat field is not a boolean or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Repeat"].get_type()));
        }

        animation_info.Repeat = key_array["Repeat"];

        if (key_array["Update"] == sol::nil || !key_array["Update"].is<sol::safe_function>()) {
            throw Exceptions::EstException(
                "[Animation] %s at key: '%s', Update field is not a function or nil but got %s",
                luaFileName.c_str(),
                key.c_str(),
                get_type_name(key_array["Update"].get_type()));
        }

        animation_info.Callback = key_array["Update"];

        return animation_info;
    } catch (const sol::error &err) {
        throw Exceptions::EstException("[Animation] %s, (key = %s)", err.what(), key.c_str());
    }
}

// Lua state misc

LuaState *LuaSkin::GetState()
{
    return &Script;
}

LuaSkin::~LuaSkin()
{
    Script.state->collect_garbage();
}

int GameLua::GetArenaIndex()
{
    return Env::GetInt("CurrentArena");
}

int GameLua::GetHitPosition()
{
    auto        manager = LuaManager::Get();
    std::string position = manager->GetSkinProp("Game", "HitPos", "0");

    return std::stoi(position);
}

int GameLua::GetLaneOffset()
{
    auto        manager = LuaManager::Get();
    std::string offset = manager->GetSkinProp("Game", "LaneOffset", "0");

    return std::stoi(offset);
}

std::tuple<int, int> GameLua::GetResolution()
{
    auto rect = Graphics::NativeWindow::Get()->GetBufferSize();

    return {
        rect.Width,
        rect.Height
    };
}

int GameLua::GetKeyCount()
{
    return Env::GetInt("KeyCount");
}

std::string GameLua::GetSkinPath()
{
    auto manager = LuaManager::Get();
    auto path = std::filesystem::path(manager->GetPath()) / __group;

    return path.string();
}

std::string GameLua::GetScriptPath()
{
    auto manager = LuaManager::Get();
    auto path = std::filesystem::path(manager->GetPath()) / "Scripts";

    return path.string();
}

std::string GameLua::GetFontPath()
{
    auto manager = LuaManager::Get();
    auto path = std::filesystem::path(manager->GetPath()) / "Fonts";

    return path.string();
}

bool GameLua::IsPathExist(std::string Path)
{
    return std::filesystem::exists(Path);
}

std::pair<int, int> GameLua::GetImageSize(std::string Path)
{
    if (!IsPathExist(Path)) {
        throw Exceptions::EstException("Path does not exist: %s", Path.c_str());
    }

    int width, height;
    int result = stbi_info(Path.c_str(), &width, &height, nullptr);

    if (result == 0) {
        throw Exceptions::EstException("Failed to get image size: %s", Path.c_str());
    }

    return std::make_pair(width, height);
}

sol::table GameLua::GetCharRange(CharRangeType type)
{
    auto      &io = ImGui::GetIO();
    sol::table result = __skin->GetState()->state->create_table();

    switch (type) {
        case CharRangeType::Default:
        {
            const ImWchar *ranges = io.Fonts->GetGlyphRangesDefault();
            for (int i = 0; i < IM_ARRAYSIZE(ranges); i += 2) {
                if (ranges[i] == 0) {
                    break;
                }

                sol::table row = __skin->GetState()->state->create_table();
                row.add(ranges[i]);
                row.add(ranges[i + 1]);

                result.add(row);
            }
            break;
        };

        case CharRangeType::Korean:
        {
            const ImWchar *ranges = io.Fonts->GetGlyphRangesKorean();
            for (int i = 0; i < IM_ARRAYSIZE(ranges); i += 2) {
                if (ranges[i] == 0) {
                    break;
                }

                sol::table row = __skin->GetState()->state->create_table();
                row.add(ranges[i]);
                row.add(ranges[i + 1]);

                result.add(row);
            }
            break;
        };

        case CharRangeType::Japanese:
        {
            const ImWchar *ranges = io.Fonts->GetGlyphRangesJapanese();
            for (int i = 0; i < IM_ARRAYSIZE(ranges); i += 2) {
                if (ranges[i] == 0) {
                    break;
                }

                sol::table row = __skin->GetState()->state->create_table();
                row.add(ranges[i]);
                row.add(ranges[i + 1]);

                result.add(row);
            }
            break;
        };

        case CharRangeType::Chinese:
        {
            const ImWchar *ranges = io.Fonts->GetGlyphRangesChineseSimplifiedCommon();
            for (int i = 0; i < IM_ARRAYSIZE(ranges); i += 2) {
                if (ranges[i] == 0) {
                    break;
                }

                sol::table row = __skin->GetState()->state->create_table();
                row.add(ranges[i]);
                row.add(ranges[i + 1]);

                result.add(row);
            }
            break;
        };
    }

    return result;
}