#include "SongSelect.h"
#include "../Game/Core/Database/GameDatabase.h"
#include "../Game/Core/Database/MusicListMaker.h"
#include "../Game/Core/Imgui/Imgui.h"
#include "../Game/Core/Skinning/LuaSkin.h"
#include "../Game/Core/Skinning/LuaManager.h"
#include "../Game/Env.h"
#include "SceneList.h"
#include <Configuration.h>
#include <Graphics/NativeWindow.h>
#include <Graphics/Renderer.h>
#include <Imgui/imgui.h>
#include <Imgui/imgui_internal.h>
#include <Screens/ScreenManager.h>

static std::array<std::string, 6>  Mods = { "Mirror", "Random", "Rearrange", "Autoplay", "Hidden", "Flashlight" };
static std::array<std::string, 14> Arena = { "Music Background", "Random",
                                             "Arena 1", "Arena 2", "Arena 3", "Arena 4", "Arena 5", "Arena 6", "Arena 7", "Arena 8", "Arena 9", "Arena 10", "Arena 11", "Arena 12" };

ImVec2 ScaleVec2(ImVec2 vec)
{
    auto window = Graphics::NativeWindow::Get();
    auto rect = window->GetBufferSize();
    auto windowNextSz = ImVec2((float)rect.Width, (float)rect.Height);

    return ImVec2(vec.x * windowNextSz.x, vec.y * windowNextSz.y);
}

ImVec2 ScaleVec2(float x, float y)
{
    return ScaleVec2(ImVec2(x, y));
}

SongSelect::SongSelect()
{
}

SongSelect::~SongSelect()
{
}

void SongSelect::Draw(double delta)
{
}

void SongSelect::Update(double delta)
{
}

bool SongSelect::Attach()
{
    Screens::Manager::DisplayFade(0, []() {});

    auto                 manager = LuaManager::Get();
    auto                 skin = manager->LoadScript(SkinGroup::SongSelect);
    auto                 fontInfo = skin->GetFont("ImGui_regular").front();
    std::vector<ImWchar> ranges;
    for (auto &it : fontInfo.CharRanges) {
        ranges.push_back(it.Start);
        ranges.push_back(it.End);
    }
    ranges.push_back(0);

    auto font = Imgui::AddFont(fontInfo.FontFile, fontInfo.Size, ranges.data());

    isScrolled = true;
    currentAlpha = 100;
    nextAlpha = 100;
    m_lastTime = 0;

    auto db = GameDatabase::Get();
    bool music_reload_required = db->FindAll().size() == 0;

    if (music_reload_required) {
        scene_index = 1;

        auto path = db->GetPath();
        if (db->FindAll().size() == 0 && std::filesystem::exists(path)) {
            m_tempMusicLists = MusicListMaker::Prepare(path);
            m_tempMusicIndex = 0;
        }
    } else {
        if (index == -1) {
            bSelectNewSong = true;
            index = db->Random().Id;
        }
    }

    if (Screens::Manager::Get()->GetCurrentScreenId() != SceneList::MainMenu) {
        // TODO: Implement this audio BGM
    }

    currentRate = Env::GetFloat("SongRate", 1.0f);
    currentSpeed = Configuration::GetFloat("Gameplay", "NoteSpeed", 1.0f);

    is_update_bgm = false;
    isWait = index != -1;
    waitTime = 0;

    return true;
}

bool SongSelect::Detach()
{
    return true;
}

void SongSelect::OnGameSelectMusic(double delta)
{
    auto music = GameDatabase::Get();
    auto window = Graphics::NativeWindow::Get();
    auto rect = window->GetBufferSize();
    auto windowNextSz = ImVec2((float)rect.Width, (float)rect.Height);
    int  currentDifficulty = Env::GetInt("Difficulty");

    // create child window
    if (ImGui::BeginChild("#Container1", ScaleVec2(ImVec2(200, 500)))) {
        if (ImGui::BeginChild("#SongSelectChild2", ScaleVec2(ImVec2(200, 200)), true)) {
            ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));

            DB_MusicItem item = {};
            if (index != -1) {
                auto it = std::find_if(m_musicList.begin(), m_musicList.end(), [&](const DB_MusicItem &item) {
                    return item.Id == index;
                });

                if (it != m_musicList.end()) {
                    item = *it;
                }
            }

            ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);

            ImGui::Text("Title\r");
            Imgui::TextBackground(color, ScaleVec2(340, 0), "%s", (const char *)item.Title);

            ImGui::Text("Artist\r");
            Imgui::TextBackground(color, ScaleVec2(340, 0), "%s", (const char *)item.Artist);

            ImGui::Text("Notecharter\r");
            Imgui::TextBackground(color, ScaleVec2(340, 0), "%s", (const char *)item.Noter);

            ImGui::Text("Note count\r");

            int difficulty = Env::GetInt("Difficulty");
            int count = item.Id == -1 ? 0 : item.MaxNotes[difficulty];
            Imgui::TextBackground(color, ScaleVec2(340, 0), "%d", count);

            ImGui::Text("BPM\r");
            float bpm = item.Id == -1 ? 0.0f : item.BPM;
            Imgui::TextBackground(color, ScaleVec2(340, 0), "%.2f", bpm);

            ImGui::PopItemFlag();
            ImGui::PopStyleVar();

            ImGui::EndChild();
        }

        if (ImGui::BeginChild("#test2", ScaleVec2(ImVec2(200, 290)), true)) {
            std::vector<std::string> difficulty = { "EX", "NX", "HX" };

            ImGui::Text("Note difficulty");
            for (int i = 0; i < difficulty.size(); i++) {
                int index = currentDifficulty;

                if (index == i) {
                    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
                }

                if (ImGui::Button(difficulty[i].c_str(), ScaleVec2(ImVec2(30, 30)))) {
                    Env::SetInt("Difficulty", i);
                }

                if (index == i) {
                    ImGui::PopItemFlag();
                    ImGui::PopStyleVar();
                }

                if (i != difficulty.size() - 1) {
                    ImGui::SameLine();
                }
            }

            ImGui::Spacing();
            ImGui::PushItemWidth(ImGui::GetCurrentWindow()->Size.x - 15);

            ImGui::Text("Notespeed");
            {
                ImGui::InputFloat("###NoteSpeed", &currentSpeed, 0.05f, 0.1f, "%.2f");
                currentSpeed = std::clamp(currentSpeed, 0.1f, 4.0f);
            }

            ImGui::Text("Rate");
            {
                ImGui::InputFloat("###Rate", &currentRate, 0.05f, 0.1f, "%.2f");
                currentRate = std::clamp(currentRate, 0.5f, 2.0f);
            }

            ImGui::PopItemWidth();

            ImGui::Text("Mods");

            for (int i = 0; i < Mods.size(); i++) {
                auto &mod = Mods[i];

                int value = Env::GetInt(mod);
                if (value == 1) {
                    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.9f);
                    ImVec4 color = ImGui::GetStyleColorVec4(ImGuiCol_Button);

                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f));
                }

                if (ImGui::Button(mod.c_str(), ScaleVec2(ImVec2(80, 0)))) {
                    Env::SetInt(mod, value == 1 ? 0 : 1);

                    switch (i) {
                        case 0:
                        {
                            Env::SetInt(Mods[1], 0);
                            Env::SetInt(Mods[2], 0);
                            break;
                        }

                        case 1:
                        {
                            Env::SetInt(Mods[0], 0);
                            Env::SetInt(Mods[2], 0);
                            break;
                        }

                        case 2:
                        {
                            bOpenRearrange = Env::GetInt(Mods[2]) == 1;

                            Env::SetInt(Mods[0], 0);
                            Env::SetInt(Mods[1], 0);
                            break;
                        }

                        case 4:
                        {
                            Env::SetInt(Mods[5], 0);
                            break;
                        }

                        case 5:
                        {
                            Env::SetInt(Mods[4], 0);
                            break;
                        }
                    }
                }

                if (value == 1) {
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();
                }

                // NewLine after 2 button on SameLine
                if ((i + 1) % 2 == 1) {
                    ImGui::SameLine();
                }
            }

            ImGui::NewLine();

            ImGui::PushItemWidth(ImGui::GetCurrentWindow()->Size.x - 15);
            ImGui::Text("Arena");

            // select
            int value = Env::GetInt("Arena") + 1;
            if (ImGui::BeginCombo("###ComboBox1Arena", Arena[value].c_str(), 0)) {
                for (int i = 0; i < Arena.size(); i++) {
                    bool is_selected = i == value;
                    if (ImGui::Selectable(Arena[i].c_str(), is_selected)) {
                        Env::SetInt("Arena", i - 1);
                    }
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ImGui::EndChild();
        }

        ImGui::EndChild();
    }

    // get current cursor pos
    ImVec2 cursorPos = ImGui::GetCursorPos();

    ImGui::SameLine();
    auto size = ImVec2(250, 575);
    auto xPos = ScaleVec2(windowNextSz.x - size.x - 5, 0);

    ImGui::SetCursorPosX(xPos.x);

    const int MAX_ROWS = 18;

    if (ImGui::BeginChild("#SongSelectChild", ScaleVec2(size), true)) {
        static char search[256] = {};
        static char previous[256] = {};

        if (ImGui::BeginChild("#SongSelectChild2", ScaleVec2(245, 500), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0));

            if (m_musicList.empty() || strcmp(search, previous) != 0) {
                memset(previous, 0, sizeof(previous));

                auto lists = music->FindQuery(search);
                m_musicList = lists;

                isScrolled = true;
                strcpy(previous, search);
            }

            for (int i = 0; i < m_musicList.size(); i++) {
                DB_MusicItem &item = m_musicList[i];

                std::string Id = "###Button" + std::to_string(i);
                bool        isSelected = item.Id == index;

                ImVec4 previousColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);

                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                }

                char buffer[256];
                sprintf(buffer, "Lv.%03d | %s", item.Difficulty[currentDifficulty], (char *)item.Title);

                std::string content = buffer + Id;
                auto        cursorPos = ImGui::GetCursorPos();

                if (ImGui::ButtonEx(
                        content.c_str(),
                        ScaleVec2(245, 25), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight)) {

                    if (item.Id != index) {
                        index = item.Id;

                        bSelectNewSong = true;
                    }
                }

                /*
                    FIXME: Goddamit!, this is hack because for some reason
                    ImGui::IsMouseClicked won't work if ImGui::ButtonEx handled it.
                */
                content += "_Context";
                if (ImGui::BeginPopupContextWindow((const char *)content.c_str())) {
                    ImGui::CloseCurrentPopup();
                    ImGui::EndPopup();

                    bOpenSongContext = true;
                }

                if (isSelected) {
                    ImGui::PopStyleColor();

                    if (isScrolled) {
                        isScrolled = false;

                        ImGui::SetScrollHereY(0.5f);
                    }
                }
            }

            ImGui::PopStyleVar();
            ImGui::EndChild();
        }

        // set cursor pos Y at bottom of window
        ImGui::SetCursorPosY(cursorPos.y - 20);
        ImGui::Separator();

        if (ImGui::BeginChild("###CHILD", ImVec2(0, 0), false, 0)) {
            ImGui::PushItemWidth(ImGui::GetWindowSize().x);
            ImGui::Text("Search:");
            ImGui::InputTextEx("###Search", "Search by Title, Artist, Noter or Id....", search, sizeof(search), ImVec2(0, 0), ImGuiInputTextFlags_AutoSelectAll);

            // if press Enter
            if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                // set focus to InputText
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::PopItemWidth();
            ImGui::EndChild();
        }

        ImGui::EndChild();
    }

    ImGui::SetCursorPos(cursorPos);
    // ImGui::PushFont(FontResources::GetButtonFont());

    if (ImGui::Button("Play", ScaleVec2(ImVec2(200, 60)))) {
        bPlay = true;
    }

    // ImGui::PopFont();

    waitTime += static_cast<float>(delta);
    const double waitTimeDelay = 0.1;

    if (ImGui::IsKeyDown(ImGuiKey_UpArrow) && waitTime >= waitTimeDelay) {
        auto it = std::find_if(m_musicList.begin(), m_musicList.end(), [&](const DB_MusicItem &a) {
            return a.Id == index;
        });

        if (--it >= m_musicList.begin()) {
            index = it->Id;
            bSelectNewSong = true;
            isScrolled = true;
        }

        waitTime = 0;
    }

    if (ImGui::IsKeyDown(ImGuiKey_DownArrow) && waitTime >= waitTimeDelay) {
        auto it = std::find_if(m_musicList.begin(), m_musicList.end(), [&](const DB_MusicItem &a) {
            return a.Id == index;
        });

        if (++it < m_musicList.end()) {
            index = it->Id;
            bSelectNewSong = true;
            isScrolled = true;
        }

        waitTime = 0;
    }
}