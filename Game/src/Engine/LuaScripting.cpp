#include "LuaScripting.h"
#include "../Data/Util/Util.hpp"
#include "../EnvironmentSetup.hpp"
#include "SkinManager.hpp" // wtf recursive

struct IGame
{
    int GetArenaIndex()
    {
        return EnvironmentSetup::GetInt("CurrentArena");
    }

    int GetHitPosition()
    {
        auto instance = SkinManager::GetInstance();
        if (!instance) {
            return -1;
        }

        auto value = instance->GetSkinProp("Game", "HitPos", "480");
        return std::stoi(value);
    }

    int GetLaneOffset()
    {
        auto instance = SkinManager::GetInstance();
        if (!instance) {
            return -1;
        }

        auto value = instance->GetSkinProp("Game", "LaneOffset", "5");
        return std::stoi(value);
    }

    std::tuple<int, int> GetResolution()
    {
        auto instance = SkinManager::GetInstance();
        if (!instance) {
            return std::make_tuple(-1, -1);
        }

        auto value = instance->GetSkinProp("Window", "NativeSize", "800x600");
        auto split = splitString(value, 'x');

        auto width = std::stoi(split[0]);
        auto height = std::stoi(split[1]);

        return std::make_tuple(width, height);
    }
};

LuaScripting::LuaScripting()
{
    Reset();
}

LuaScripting::LuaScripting(std::filesystem::path lua_dir_path) : LuaScripting::LuaScripting()
{
    m_lua_dir_path = lua_dir_path;
}

LuaScripting::~LuaScripting()
{
    m_dispose = true;
    Reset();
}

void LuaScripting::Reset()
{
    for (auto &[group, state] : m_states) {
        if (state.game_state) {
            delete state.game_state;
        }
    }

    if (m_arena_states) {
        if (m_arena_states->game_state) {
            delete m_arena_states->game_state;
        }
    }

    m_states.clear();
    m_arena_states.reset();

    if (m_dispose) {
        return;
    }

    m_expected_files = {
        { SkinGroup::SongSelect, "SongSelect.lua" },
        { SkinGroup::MainMenu, "MainMenu.lua" },
        { SkinGroup::Playing, "Playing.lua" },
        { SkinGroup::Notes, "Notes.lua" }
    };
}

sol::table LuaScripting::LoadLua(sol::state &state, std::filesystem::path path)
{
    sol::table result = state.script_file(path.string(), sol::load_mode::text);

    return result;
}

void SetupLuaLibrary(ScriptState &script)
{
    script.state.open_libraries(sol::lib::base, sol::lib::package);

    sol::table HeaderType = script.state.create_table();
    HeaderType["Playing"] = SkinGroup::Playing;
    HeaderType["SongSelect"] = SkinGroup::SongSelect;
    HeaderType["MainMenu"] = SkinGroup::MainMenu;
    HeaderType["Notes"] = SkinGroup::Notes;

    sol::table DataType = script.state.create_table();
    DataType["Numeric"] = SkinDataType::Numeric;
    DataType["Position"] = SkinDataType::Position;
    DataType["Rect"] = SkinDataType::Rect;
    DataType["Note"] = SkinDataType::Note;
    DataType["Sprite"] = SkinDataType::Sprite;

    script.state["HeaderType"] = HeaderType;
    script.state["DataType"] = DataType;

    script.state.new_usertype<IGame>("IGame",
                                     "new", sol::no_constructor,
                                     "GetArenaIndex", &IGame::GetArenaIndex,
                                     "GetHitPosition", &IGame::GetHitPosition,
                                     "GetLaneOffset", &IGame::GetLaneOffset,
                                     "GetResolution", &IGame::GetResolution);

    script.game_state = new IGame();
    script.state["Game"] = script.game_state;
}

void GetLuaState(ScriptState &state, sol::table &table)
{
    if (table["type"].get_type() != sol::type::number) {
        throw std::runtime_error("Script didn't return the expected value");
    }

    state.type = table["type"];

    if (table["init"].get_type() != sol::type::function) {
        throw std::runtime_error("Script did not have init function");
    }

    state.init = table["init"];
}


void LuaScripting::TryLoadGroup(SkinGroup group) // Not fully testes unless someone want make skin with lua script
{
    std::filesystem::path fullPath;

    if (group == SkinGroup::Notes && EnvironmentSetup::GetInt("NoteSkin") != 2) {
        auto path = std::filesystem::current_path() / "Resources";
        if (EnvironmentSetup::GetInt("NoteSkin") == 1) {
            fullPath = path / "Scripts" / "Notes.lua";
        }
        else {
            fullPath = path / "Scripts" / "Notes.lua";
        }
    }
    else {
        fullPath = m_lua_dir_path / m_expected_files[group];
    }

    if (!std::filesystem::exists(fullPath)) {
        throw std::runtime_error("Missing file: " + fullPath.string());
    }

    m_states[group] = {};
    auto& state = m_states[group];
    state.state = sol::state();

    if (state.state.lua_state() == nullptr) {
        throw std::runtime_error("Failed to create lua state");
    }

    SetupLuaLibrary(state);
    sol::table script = LoadLua(state.state, fullPath);
    GetLuaState(state, script);
}

void LuaScripting::TryLoadArena()
{
    auto fullPath = m_lua_dir_path / "Arena.lua";
    if (!std::filesystem::exists(fullPath)) {
        throw std::runtime_error("Missing file: " + fullPath.string());
    }

    m_arena_states = std::make_unique<ScriptState>();
    m_arena_states->state = {};

    if (m_arena_states->state.lua_state() == nullptr) {
        throw std::runtime_error("Failed to create lua state");
    }

    SetupLuaLibrary(*m_arena_states);
    sol::table script = LoadLua(m_arena_states->state, fullPath);

    GetLuaState(*m_arena_states.get(), script);
}

void LuaScripting::Update(double delta)
{
    for (auto &[key, value] : m_states) {
        value.update(delta);
    }
}

std::vector<NumericValue> LuaScripting::GetNumeric(SkinGroup group, std::string key)
{
    try {
        if (m_states[group].init.lua_state() == NULL) {
            throw std::runtime_error("Lua state is null");
        }

        sol::table result_table = m_states[group].init();
        sol::table key_array = result_table[SkinDataType::Numeric][key];

        std::vector<NumericValue> result;
        for (auto &value : key_array) {
            NumericValue numeric_value = {};

            sol::table value_table = value.second;
            numeric_value.X = value_table[1];
            numeric_value.Y = value_table[2];
            numeric_value.MaxDigit = value_table[3];
            std::string direction = value_table[4];
            std::transform(direction.begin(), direction.end(), direction.begin(), ::tolower);

            if (direction == "mid")
                numeric_value.Direction = 0;
            else if (direction == "left")
                numeric_value.Direction = -1;
            else if (direction == "right")
                numeric_value.Direction = 1;
            numeric_value.FillWithZero = value_table[5];

            result.push_back(numeric_value);
        }

        return result;
    } catch (const sol::error &err) {
        throw std::runtime_error(err.what());
    }
}

std::vector<PositionValue> LuaScripting::GetPosition(SkinGroup group, std::string key, int KeyCount)
{
    if (m_states.find(group) == m_states.end()) {
        TryLoadGroup(group);
    }

    try {
        if (m_states[group].init.lua_state() == NULL) {
            throw std::runtime_error("Lua state is null");
        }

        sol::table result_table = m_states[group].init();
        sol::table key_array = result_table[SkinDataType::Position][KeyCount][key];

        std::vector<PositionValue> result;
        for (auto &value : key_array) {
            PositionValue position_value = {};

            sol::table value_table = value.second;
            position_value.X = value_table[1];
            position_value.Y = value_table[2];
            position_value.AnchorPointX = value_table[3];
            position_value.AnchorPointY = value_table[4];
            position_value.RGB[0] = value_table[5];
            position_value.RGB[1] = value_table[6];
            position_value.RGB[2] = value_table[7];

            result.push_back(position_value);
        }

        return result;
    } catch (const sol::error &err) {
        throw std::runtime_error(err.what());
    }
}

std::vector<RectInfo> LuaScripting::GetRect(SkinGroup group, std::string key)
{
    if (m_states.find(group) == m_states.end()) {
        TryLoadGroup(group);
    }

    try {
        if (m_states[group].init.lua_state() == NULL) {
            throw std::runtime_error("Lua state is null");
        }

        std::vector<RectInfo> result;

        sol::table result_table = m_states[group].init();
        sol::table key_array = result_table[SkinDataType::Rect][key];

        for (auto &value : key_array) {
            RectInfo rect_info = {};

            sol::table value_table = value.second;
            rect_info.X = value_table[1];
            rect_info.Y = value_table[2];
            rect_info.Width = value_table[3];
            rect_info.Height = value_table[4];

            result.push_back(rect_info);
        }

        return result;
    } catch (const sol::error &err) {
        throw std::runtime_error(err.what());
    }
}

NoteValue LuaScripting::GetNote(SkinGroup group, std::string key, int KeyCount)
{
    if (m_states.find(group) == m_states.end()) {
        TryLoadGroup(group);
    }

    try {
        if (m_states[group].init.lua_state() == NULL) {
            throw std::runtime_error("Lua state is null");
        }

        sol::table result_table = m_states[group].init();
        sol::table key_array = result_table[SkinDataType::Note][KeyCount][key];

        NoteValue note_value = {};
        note_value.numOfFiles = key_array[1];
        note_value.fileName = key_array[2];

        return note_value;
    } catch (const sol::error &err) {
        throw std::runtime_error(err.what());
    }
}

SpriteValue LuaScripting::GetSprite(SkinGroup group, std::string key)
{
    if (m_states.find(group) == m_states.end()) {
        TryLoadGroup(group);
    }

    try {
        if (m_states[group].init.lua_state() == NULL) {
            throw std::runtime_error("Lua state is null");
        }

        sol::table result_table = m_states[group].init();
        sol::table key_array = result_table[SkinDataType::Sprite][key];

        SpriteValue sprite_value = {};
        sprite_value.X = key_array[1];
        sprite_value.Y = key_array[2];
        sprite_value.AnchorPointX = key_array[3];
        sprite_value.AnchorPointY = key_array[4];
        sprite_value.FrameTime = key_array[5];

        return sprite_value;
    } catch (const sol::error &err) {
        throw std::runtime_error(err.what());
    }
}

