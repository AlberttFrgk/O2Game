﻿#include "Settings.h"
#include <Configuration.h>
#include <Imgui/ImguiUtil.h>
#include <Imgui/imgui.h>
#include <Rendering/Window.h>
#include <SDL2/SDL.h>
#include <SceneManager.h>
#include <Texture/MathUtils.h>

#include "../../Data/Util/Util.hpp"
#include "../../Engine/SkinManager.hpp"
#include "../../EnvironmentSetup.hpp"
#include <array>
#include <map>

SettingsOverlay::SettingsOverlay()
{
    m_name = "Settings";
    m_position = MathUtil::ScaleVec2(450, 450);
    m_size = ImVec2(0, 0);

    for (auto &dir : std::filesystem::directory_iterator(std::filesystem::current_path() / "Skins")) {
        if (dir.is_directory()) {
            if (std::filesystem::exists(dir.path() / "GameSkin.ini")) {
                skins.push_back(dir.path().filename().string());
            }
        }
    }

    LoadConfiguration();
}

static std::map<int, std::string> Graphics = {
    { 0, "OpenGL" },
    { 1, "Vulkan" },
#if _WIN32
    { 2, "DirectX-9" },
    { 3, "DirectX-12" },
#endif
#if __APPLE__
    { 4, "Metal" },
#endif
};

//#if _WIN32
//{ 2, "DirectX-9" },
//{ 3, "DirectX-11" },
//{ 4, "DirectX-12" },
//#endif
//#if __APPLE__
//{ 5, "Metal" },
//#endif
//};

static std::array<std::string, 4>  LongNote = { "None", "Short", "Normal", "Long" };
//static std::array<std::string, 14> m_fps = { "30", "60", "75", "120", "144", "165", "180", "240", "360", "480", "600", "800", "1000", "Unlimited" };
static std::array<std::string, 3>  SelectedBackground = { "Arena", "Song", "Disable" };
static std::array<std::string, 3>  NoteSkin = { "Square", "Circle", "Custom" };
static std::vector<std::string> m_resolutions = {};

int currentFPSIndex = 0;
int customFPS = 0;

namespace {
    int GetScreenRefreshRate() {
        SDL_DisplayMode displayMode;
        if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
            return 60; // Default to 60 if SDL can't catch refresh rate (pc issues)
        }
        return displayMode.refresh_rate;
    }

    std::vector<std::string> GetFpsOptions() {
        int refreshRate = GetScreenRefreshRate();
        std::vector<int> multipliers = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 14, 16, 20 };
        std::vector<std::string> fpsOptions;

        for (int multiplier : multipliers) {
            fpsOptions.push_back(std::to_string(refreshRate * multiplier));
        }
        fpsOptions.push_back("Unlimited");
        return fpsOptions;
    }
}

void SettingsOverlay::Render(double delta)
{
    auto &io = ImGui::GetIO();

    int  sceneIndex = EnvironmentSetup::GetInt("Setting_SceneIndex");
    bool changeResolution = false;

    switch (sceneIndex) {
        case 1:
        {
            auto key = EnvironmentSetup::Get("Scene_KbKey");
            ImGui::Text("%s", "Waiting for keybind input...");
            ImGui::Text("%s", ("Press any key to set the Key: " + key).c_str());

            std::map<ImGuiKey, bool> blacklistedKey = {
                // Space, Enter
                { ImGuiKey_Space, true },
                { ImGuiKey_Enter, true },
                { ImGuiKey_KeyPadEnter, true },
            };

            std::string keyCount = EnvironmentSetup::Get("Scene_KbLaneCount");
            for (int i = 0; i < IM_ARRAYSIZE(io.KeysDown); i++) {
                if (io.KeysDown[i] && !blacklistedKey[(ImGuiKey)i]) {
                    EnvironmentSetup::SetInt("Setting_SceneIndex", 0);

                    auto        ikey = SDL_GetKeyFromScancode((SDL_Scancode)i);
                    std::string name = SDL_GetKeyName(ikey);

                    Configuration::Set("KeyMapping", keyCount + "Lane" + key, name);
                    break;
                }
            }
            break;
        }

        case 2:
        {
            ImGui::Text("You sure you want reset the settings?");
            ImGui::Text("This will reset all settings to default.");

            bool done = false;

            if (ImGui::Button("Yes###ButtonPrompt1", MathUtil::ScaleVec2(ImVec2(40, 0)))) {
                done = true;

                Configuration::ResetConfiguration();
                LoadConfiguration();
            }

            ImGui::SameLine();

            if (ImGui::Button("No###ButtonPrompt2", MathUtil::ScaleVec2(ImVec2(40, 0)))) {
                done = true;
            }

            if (done) {
                EnvironmentSetup::SetInt("Setting_SceneIndex", 0);
            }
            break;
        }

        default:
        {
            auto childSettingVec2 = MathUtil::ScaleVec2(ImVec2(400, 250));

            if (ImGui::BeginChild("###ChildSettingWnd", MathUtil::ScaleVec2(ImVec2(400, 250)))) {
                if (ImGui::BeginTabBar("OptionTabBar")) {
                    if (ImGui::BeginTabItem("Inputs")) {
                        ImGui::Text("Keys Configuration");
                        for (int i = 0; i < 7; i++) {
                            if (i != 0) {
                                ImGui::SameLine();
                            }

                            std::string currentKey = Configuration::Load("KeyMapping", "Lane" + std::to_string(i + 1));
                            if (ImGui::Button((currentKey + "###7KEY" + std::to_string(i)).c_str(), MathUtil::ScaleVec2(ImVec2(50, 0)))) {
                                EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
                                EnvironmentSetup::Set("Scene_KbLaneCount", "");
                                EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
                            }
                        }

                        ImGui::NewLine();

                        /*ImGui::Text("6 Keys Configuration");
                        for (int i = 0; i < 6; i++) {
                            if (i != 0) {
                                ImGui::SameLine();
                            }

                            std::string currentKey = Configuration::Load("KeyMapping", "6_Lane" + std::to_string(i + 1));
                            if (ImGui::Button((currentKey + "###6KEY" + std::to_string(i)).c_str(), MathUtil::ScaleVec2(ImVec2(50, 0)))) {
                                EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
                                EnvironmentSetup::Set("Scene_KbLaneCount", "6_");
                                EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
                            }
                        }

                        ImGui::NewLine();

                        ImGui::Text("5 Keys Configuration");
                        for (int i = 0; i < 5; i++) {
                            if (i != 0) {
                                ImGui::SameLine();
                            }

                            std::string currentKey = Configuration::Load("KeyMapping", "5_Lane" + std::to_string(i + 1));
                            if (ImGui::Button((currentKey + "###5KEY" + std::to_string(i)).c_str(), MathUtil::ScaleVec2(ImVec2(50, 0)))) {
                                EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
                                EnvironmentSetup::Set("Scene_KbLaneCount", "5_");
                                EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
                            }
                        }

                        ImGui::NewLine();

                        ImGui::Text("4 Keys Configuration");
                        for (int i = 0; i < 4; i++) {
                            if (i != 0) {
                                ImGui::SameLine();
                            }

                            std::string currentKey = Configuration::Load("KeyMapping", "4_Lane" + std::to_string(i + 1));
                            if (ImGui::Button((currentKey + "###4KEY" + std::to_string(i)).c_str(), MathUtil::ScaleVec2(ImVec2(50, 0)))) {
                                EnvironmentSetup::SetInt("Setting_SceneIndex", 1);
                                EnvironmentSetup::Set("Scene_KbLaneCount", "4_");
                                EnvironmentSetup::Set("Scene_KbKey", std::to_string(i + 1));
                            }
                        }*/

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Graphics")) {
                        int GraphicsIndex = 0;
                        int nextGraphicsIndex = -1;
                        try {
                            GraphicsIndex = std::stoi(Configuration::Load("Game", "Renderer").c_str());
                        }
                        catch (const std::invalid_argument&) {
                        }

                        ImGui::Text("Graphics");
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "You need restart game after setting this!");
                        auto it = Graphics.find(GraphicsIndex);
                        if (it == Graphics.end()) {
                            GraphicsIndex = 0;
                        }

                        if (ImGui::BeginCombo("###ComboBox1", Graphics[GraphicsIndex].c_str())) {
                            for (int i = 0; i < Graphics.size(); i++) {
                                bool isSelected = (GraphicsIndex == i);

                                if (ImGui::Selectable(Graphics[i].c_str(), &isSelected)) {
                                    nextGraphicsIndex = i;
                                }

                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndCombo();
                        }

                        if (nextGraphicsIndex != -1 && nextGraphicsIndex != GraphicsIndex) {
                            Configuration::Set("Game", "Renderer", std::to_string(nextGraphicsIndex));
                        }

                        int currentResolutionIndexRn = currentResolutionIndex;

                        ImGui::Text("Resolution");
                        if (ImGui::BeginCombo("###ComboBox3", m_resolutions[currentResolutionIndex].c_str())) {
                            for (int i = 0; i < m_resolutions.size(); i++) {
                                bool isSelected = (currentResolutionIndex == i);

                                if (ImGui::Selectable(m_resolutions[i].c_str(), &isSelected)) {
                                    currentResolutionIndex = i;
                                }

                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndCombo();
                        }

                        if (currentResolutionIndex != currentResolutionIndexRn) {
                            changeResolution = true;
                            Configuration::Set("Game", "Resolution", m_resolutions[currentResolutionIndex]);
                        }

                        ImGui::NewLine();

                        std::vector<std::string> fpsOptions = GetFpsOptions();
                        ImGui::Text("FPS");
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Warning: setting unlimited FPS can cause PC to become unstable!");
                        if (ImGui::BeginCombo("###ComboBox2", fpsOptions[currentFPSIndex].c_str())) {
                            for (int i = 0; i < fpsOptions.size(); i++) {
                                bool isSelected = (currentFPSIndex == i);

                                if (ImGui::Selectable(fpsOptions[i].c_str(), &isSelected)) {
                                    currentFPSIndex = i;
                                    if (fpsOptions[i] == "Unlimited") {
                                        customFPS = 9999;
                                        Configuration::Set("Game", "FrameLimit", std::to_string(customFPS));
                                    }
                                    else {
                                        customFPS = 0;
                                        Configuration::Set("Game", "FrameLimit", fpsOptions[i]);
                                    }
                                }

                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::EndTabItem();

                    }

                    if (ImGui::BeginTabItem("Audio")) {
                        ImGui::Text("Audio Volume");
                        ImGui::SliderInt("###Slider2", &currentVolume, 0, 100);

                        ImGui::NewLine();

                        ImGui::Text("Audio Offset");
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Warning: this will make keysounded sample as auto sample!");
                        ImGui::SliderInt("###Slider1", &currentOffset, -1000, 1000);

                        ImGui::NewLine();
                        ImGui::Checkbox("Convert Sample to Auto Sample###Checkbox1", &convertAutoSound);
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Convert all keysounded sample to auto sample");

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Game")) {
                        ImGui::Text("Guide Line Length");

                        for (int i = (int)LongNote.size() - 1; i >= 0; i--) {
                            bool selected = currentGuideLineIndex == i;

                            ImGui::PushItemWidth(50);
                            if (ImGui::Checkbox(("###ComboCheck" + std::to_string(i)).c_str(), &selected)) {
                                currentGuideLineIndex = i;
                            }

                            ImGui::SameLine();
                            ImGui::Text("%s", LongNote[i].c_str());
                            ImGui::SameLine();
                            ImGui::Dummy(MathUtil::ScaleVec2(ImVec2(25, 0)));
                            ImGui::SameLine();

                            if (i < LongNote.size()) {
                                ImGui::SameLine();
                            }
                        }

                        ImGui::NewLine();
                        ImGui::NewLine();

                        ImGui::Text("Gameplay-Related Configuration");

                        ImGui::Checkbox("New Measure Line###SetCheckbox1", &MeasureLineType);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Measure line position in the middle of the note; otherwise, it will be at the bottom of the note");
                        }
                        if (MeasureLineType) {
                            EnvironmentSetup::SetInt("MeasureLineType", 1);
                        }
                        else {
                            EnvironmentSetup::SetInt("MeasureLineType", 0);
                        }

                        ImGui::SameLine();

                        ImGui::Checkbox("New Long Note###SetCheckbox3", &NewLongNote);
                        if (NewLongNote) { // Leave everything untouched since no reason to make whole changes
                            EnvironmentSetup::SetInt("NewLN", 1);
                        }
                        else {
                            EnvironmentSetup::SetInt("NewLN", 0);
                        }

                        ImGui::Checkbox("Disable Measure Line###SetCheckbox2", &MeasureLine);
                        if (MeasureLine) {
                            EnvironmentSetup::SetInt("MeasureLine", 0);
                        }
                        else {
                            EnvironmentSetup::SetInt("MeasureLine", 1);
                        }

                        ImGui::SameLine();

                        ImGui::Checkbox("Long Note Body On Top###SetCheckbox3", &LNBodyOnTop);
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("If your note skin has issues, enable this option!");
                        }
                        if (LNBodyOnTop) {
                            EnvironmentSetup::SetInt("LNBodyOnTop", 1);
                        }
                        else {
                            EnvironmentSetup::SetInt("LNBodyOnTop", 0);
                        }

                        ImGui::EndTabItem();
                    }

                    if (ImGui::BeginTabItem("Skins")) {
                        ImGui::Text("Current selected skin: ");
                        if (ImGui::BeginCombo("##SkinsComboBox", currentSkin.c_str())) {
                            for (int i = 0; i < skins.size(); i++) {
                                bool isSelected = (currentSkin == skins[i]);

                                if (ImGui::Selectable(skins[i].c_str(), &isSelected)) {
                                    currentSkin = skins[i];
                                }

                                if (isSelected) {
                                    ImGui::SetItemDefaultFocus();
                                }
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::NewLine();

                        ImGui::Text("Note Skin");
                        for (int i = 0; i < NoteSkin.size(); ++i) {
                            bool selected = (NoteIndex == i);

                            std::string tooltipText;
                            if (i == 0) {
                                //tooltipText = "";
                            }
                            else if (i == 1) {
                                //tooltipText = "";
                            }
                            else if (i == 2) {
                                tooltipText = "Using custom note image from skin folder";
                            }

                            if (ImGui::Checkbox(NoteSkin[i].c_str(), &selected)) {
                                NoteIndex = i;
                            }

                            if (!tooltipText.empty() && ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("%s", tooltipText.c_str());
                            }

                            ImGui::SameLine();
                        }

                        if (NoteIndex == 0) {
                            EnvironmentSetup::SetInt("NoteSkin", 0);
                        }
                        else if (NoteIndex == 1) {
                            EnvironmentSetup::SetInt("NoteSkin", 1);
                        }
                        else if (NoteIndex == 2) {
                            EnvironmentSetup::SetInt("NoteSkin", 2);
                        }

                        ImGui::NewLine();
                        ImGui::NewLine();

                        ImGui::Text("Background");
                        for (int i = 0; i < SelectedBackground.size(); ++i) {
                            bool selected = (BackgroundIndex == i);

                            std::string tooltipText;
                            if (i == 0) {
                                tooltipText = "Using arena image background inside Skin folder";
                            }
                            else if (i == 1) {
                                tooltipText = "Using song image background inside O2Jam file / BMS folder / osu!mania beatmap folder";
                            }
                            else if (i == 2) {
                                tooltipText = "Not using any background";
                            }

                            if (ImGui::Checkbox(SelectedBackground[i].c_str(), &selected)) {
                                BackgroundIndex = i;
                            }

                            if (!tooltipText.empty() && ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("%s", tooltipText.c_str());
                            }

                            ImGui::SameLine();
                        }

                        if (BackgroundIndex == 1) {
                            EnvironmentSetup::SetInt("Background", 1);
                        }
                        else if (BackgroundIndex == 2) {
                            EnvironmentSetup::SetInt("Background", 2);
                        }
                        else {
                            EnvironmentSetup::SetInt("Background", 0);
                        }

                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }

                ImGui::EndChild();
            }

            ImGui::NewLine();

            if (ImGui::Button("Reset###Setting1", MathUtil::ScaleVec2(50, 0))) {
                EnvironmentSetup::SetInt("Setting_SceneIndex", 2);
            }

            ImGui::SameLine();

            if (ImGui::Button("OK###Setting2", MathUtil::ScaleVec2(50, 0))) {
                bSave = true;
                m_exit = true;
            }
            break;
        }
    }

    if (changeResolution) {
        std::vector<std::string> resolution = splitString(m_resolutions[currentResolutionIndex], 'x');
        int                      x = 800, y = 600;

        try {
            x = std::stoi(resolution[0]);
            y = std::stoi(resolution[1]);
        } catch (const std::invalid_argument &) {
        }

        GameWindow::GetInstance()->ResizeWindow(
            x, y);
    }
}

bool SettingsOverlay::Attach()
{
    LoadConfiguration();

    m_exit = false;
    return true;
}

bool SettingsOverlay::Detach()
{
    ImGui::CloseCurrentPopup();
    SaveConfiguration();

    return true;
}

void SettingsOverlay::LoadConfiguration()
{
    if (m_resolutions.size() == 0) {
        int displayCount = SDL_GetNumDisplayModes(0);
        for (int i = 0; i < displayCount; i++) {
            SDL_DisplayMode mode;
            SDL_GetDisplayMode(0, i, &mode);

            m_resolutions.push_back(std::to_string(mode.w) + "x" + std::to_string(mode.h));
        }

        // remove duplicate field
        m_resolutions.erase(std::unique(m_resolutions.begin(), m_resolutions.end()), m_resolutions.end());

        GameWindow *window = GameWindow::GetInstance();
        std::string currentResolution = std::to_string(window->GetWidth()) + "x" + std::to_string(window->GetHeight());

        // find index
        currentResolutionIndex = (int)(std::find(m_resolutions.begin(), m_resolutions.end(), currentResolution) - m_resolutions.begin());
    }

    try {
        currentOffset = std::stoi(Configuration::Load("Game", "AudioOffset").c_str());
    } catch (const std::invalid_argument &) {
        currentOffset = 0;
    }

    try {
        currentVolume = std::stoi(Configuration::Load("Game", "AudioVolume").c_str());
    } catch (const std::invalid_argument &) {
        currentVolume = 0;
    }

    try {
        convertAutoSound = std::stoi(Configuration::Load("Game", "AutoSound").c_str()) != 0;
    } catch (const std::invalid_argument &) {
        convertAutoSound = true;
    }

    try {
        std::string frameLimit = Configuration::Load("Game", "FrameLimit");
        auto fpsOptions = GetFpsOptions();
        auto it = std::find(fpsOptions.begin(), fpsOptions.end(), frameLimit);
        if (it != fpsOptions.end()) {
            currentFPSIndex = static_cast<int>(std::distance(fpsOptions.begin(), it));
            customFPS = 0;
        }
        else {
            currentFPSIndex = static_cast<int>(fpsOptions.size()) - 1;
            customFPS = 9999;
        }
    }
    catch (const std::invalid_argument&) {
        customFPS = 9999;
    }

    SceneManager::GetInstance()->SetFrameLimit(std::atof(Configuration::Load("Game", "FrameLimit").c_str()));

    try {
        currentGuideLineIndex = std::stoi(Configuration::Load("Game", "GuideLine").c_str());
    } catch (const std::invalid_argument &) {
        currentGuideLineIndex = 2;
    }

    try {
        BackgroundIndex = std::stoi(Configuration::Load("Game", "Background").c_str());
    } catch (const std::invalid_argument&) {
        BackgroundIndex = 0;
    }

    if (BackgroundIndex == 1) {
        EnvironmentSetup::SetInt("Background", 1);
    }
    else if (BackgroundIndex == 2) {
        EnvironmentSetup::SetInt("Background", 2);
    }
    else {
        EnvironmentSetup::SetInt("Background", 0);
    }

    try {
        NoteIndex = std::stoi(Configuration::Load("Game", "NoteSkin").c_str());
    }
    catch (const std::invalid_argument&) {
        NoteIndex = 2;
    }

    if (NoteIndex == 0) {
        EnvironmentSetup::SetInt("NoteSkin", 0);
    }
    else if (NoteIndex == 1) {
        EnvironmentSetup::SetInt("NoteSkin", 1);
    }
    else if (NoteIndex == 2) {
        EnvironmentSetup::SetInt("NoteSkin", 2);
    }

    try {
        int NewLNValue = std::stoi(Configuration::Load("Game", "NewLongNote"));
        NewLongNote = (NewLNValue == 1);
    }
    catch (const std::invalid_argument&) {
        NewLongNote = false;
    }

    if (NewLongNote) {
        EnvironmentSetup::SetInt("NewLN", 1);
    }
    else {
        EnvironmentSetup::SetInt("NewLN", 0);
    }

    try {
        int MeasureLineTypeValue = std::stoi(Configuration::Load("Game", "MeasureLineType"));
        MeasureLineType = (MeasureLineTypeValue == 1);
    }
    catch (const std::invalid_argument&) {
        MeasureLineType = false;
    }

    if (MeasureLineType) {
        EnvironmentSetup::SetInt("MeasureLineType", 1);
    }
    else {
        EnvironmentSetup::SetInt("MeasureLineType", 0);
    }

    try {
        int measureLineValue = std::stoi(Configuration::Load("Game", "MeasureLine"));
        MeasureLine = (measureLineValue == 1);
    }
    catch (const std::invalid_argument&) {
        MeasureLine = true;
    }

    if (MeasureLine) {
        EnvironmentSetup::SetInt("MeasureLine", 0);
    }
    else {
        EnvironmentSetup::SetInt("MeasureLine", 1);
    }

    try {
        int LNBodyOnTopValue = std::stoi(Configuration::Load("Game", "LNBodyOnTop"));
        LNBodyOnTop = (LNBodyOnTopValue == 1);
    }
    catch (const std::invalid_argument&) {
        LNBodyOnTop = true;
    }

    if (LNBodyOnTop) {
        EnvironmentSetup::SetInt("LNBodyOnTop", 1);
    }
    else {
        EnvironmentSetup::SetInt("LNBodyOnTop", 0);
    }

    currentSkin = Configuration::Load("Game", "Skin");
    PreloadSkin();
}

void SettingsOverlay::SaveConfiguration()
{
    Configuration::Set("Game", "AudioOffset", std::to_string(currentOffset));
    Configuration::Set("Game", "AudioVolume", std::to_string(currentVolume));
    Configuration::Set("Game", "AutoSound", std::to_string(convertAutoSound ? 1 : 0));
    Configuration::Set("Game", "GuideLine", std::to_string(currentGuideLineIndex));
    Configuration::Set("Game", "NoteSkin", std::to_string(NoteIndex));
    Configuration::Set("Game", "Background", std::to_string(BackgroundIndex));
    Configuration::Set("Game", "MeasureLine", std::to_string(MeasureLine ? 1 : 0));
    Configuration::Set("Game", "MeasureLineType", std::to_string(MeasureLineType ? 1 : 0));
    Configuration::Set("Game", "LNBodyOnTop", std::to_string(LNBodyOnTop ? 1 : 0));
    Configuration::Set("Game", "NewLongNote", std::to_string(NewLongNote ? 1 : 0));

    if (currentFPSIndex == GetFpsOptions().size() - 1 && customFPS > 0) {
        Configuration::Set("Game", "FrameLimit", std::to_string(customFPS));
    }
    else {
        Configuration::Set("Game", "FrameLimit", GetFpsOptions()[currentFPSIndex]);
    }

    SceneManager::GetInstance()->SetFrameLimit(std::atof(Configuration::Load("Game", "FrameLimit").c_str()));

    if (currentSkin.empty()) {
        throw std::runtime_error("SKIN_NAME Undefined!");
    }

    Configuration::Set("Game", "Skin", currentSkin);
    PreloadSkin();
}

void SettingsOverlay::PreloadSkin()
{
    auto manager = SkinManager::GetInstance();
    manager->LoadSkin(currentSkin);

    std::string resolution = manager->GetSkinProp("Window", "NativeSize", "800x600");
    if (resolution.empty()) {
        resolution = "800x600";
    }

    std::vector<std::string> resolutionVec = splitString(resolution, 'x');
    int                      x = 800, y = 600;

    try {
        x = std::stoi(resolutionVec[0]);
        y = std::stoi(resolutionVec[1]);
    } catch (const std::invalid_argument &) {
    }

    GameWindow::GetInstance()->ResizeBuffer(
        x, y);
}
