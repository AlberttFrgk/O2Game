#include "LoadingScene.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "Configuration.h"
#include "Exception/SDLException.h"
#include "MsgBox.h"
#include "Rendering/Window.h"
#include "SceneManager.h"

#include "../Data/Chart.hpp"
#include "../Data/osu.hpp"
#include "../Engine/SkinManager.hpp"

#include "../EnvironmentSetup.hpp"
#include "../GameScenes.h"
#include "../Resources/GameDatabase.h"

LoadingScene::LoadingScene()
{
    m_background = nullptr;
    m_counter = 0;
}

LoadingScene::~LoadingScene()
{
}

void LoadingScene::Update(double delta)
{
    SkinManager::GetInstance()->ReloadSkin();

    if (is_ready || fucked)
        m_counter += delta;
    int  songId = EnvironmentSetup::GetInt("Key");
    int  diffIndex = EnvironmentSetup::GetInt("Difficulty");
    bool IsO2Jam = false;

    Chart *chart = (Chart *)EnvironmentSetup::GetObj("SONG");
    if (chart == nullptr || chart->GetO2JamId() != songId) {
        if (!fucked) {
            std::filesystem::path file;

            if (songId != -1) {
                file = GameDatabase::GetInstance()->GetPath();
                file /= "o2ma" + std::to_string(songId) + ".ojn";
            } else {
                file = EnvironmentSetup::GetPath("FILE");
                IsFile = true;

                auto autoplay = EnvironmentSetup::GetInt("ParameterAutoplay");
                auto rate = EnvironmentSetup::Get("ParameterRate");

                EnvironmentSetup::SetInt("Autoplay", autoplay);
                EnvironmentSetup::Set("SongRate", rate);

                if (EnvironmentSetup::GetInt("FileOpen") == 1) {
                    LoadModifiers();
                }
                else {
                    EnvironmentSetup::SetInt("Song BG", 1);
                }
            }

            const char *bmsfile[] = { ".bms", ".bme", ".bml", ".bmsc" };
            const char *ojnfile = ".ojn";

            if (file.extension() == bmsfile[0] || file.extension() == bmsfile[1] || file.extension() == bmsfile[2] || file.extension() == bmsfile[3]) {
                BMS::BMSFile beatmap;
                beatmap.Load(file);

                if (!beatmap.IsValid()) {
                    MsgBox::Show("FailChart", "Error", "Failed to BMS chart!", MsgBoxType::OK);
                    fucked = true;
                    return;
                }

                chart = new Chart(beatmap);
                EnvironmentSetup::SetInt("SongType", 0);
            } else if (file.extension() == ojnfile) {
                O2::OJN o2jamFile;
                o2jamFile.Load(file);

                if (!o2jamFile.IsValid()) {
                    std::string msg = "Failed to load OJN: " + ("o2ma" + std::to_string(songId) + ".ojn");

                    MsgBox::Show("FailChart", "Error", msg.c_str(), MsgBoxType::OK);
                    fucked = true;
                    return;
                }

                chart = new Chart(o2jamFile, diffIndex);
                EnvironmentSetup::SetInt("SongType", 1);
                IsO2Jam = true;

            } else {
                Osu::Beatmap beatmap(file);

                if (!beatmap.IsValid()) {
                    MsgBox::Show("FailChart", "Error", "Failed to load osu beatmap!", MsgBoxType::OK);
                    fucked = true;
                    return;
                }

                chart = new Chart(beatmap);
                EnvironmentSetup::SetInt("SongType", 2);
            }

            EnvironmentSetup::SetObj("SONG", chart);
        }
    } else {
        IsO2Jam = chart->GetO2JamId() == songId; // TODO: refactor this
    }

    std::filesystem::path dirPath = chart->m_beatmapDirectory;
    dirPath /= chart->m_backgroundFile;

    if (IsO2Jam && !IsFile) {
        auto item = GameDatabase::GetInstance()->Find(songId);

        bool hashFound = strcmp(item.Hash[diffIndex], chart->MD5Hash.c_str()) == 0;
        if (!hashFound && !IsFile) {
            MsgBox::Show("FailChart", "Error", "Invalid map identifier, please refresh music list using F5 key!", MsgBoxType::OK);
            fucked = true;
        }
    }

    if (!fucked) {
        try {
            if (m_background == nullptr) {
                GameWindow *window = GameWindow::GetInstance();
                if (chart->m_backgroundFile.size() > 0 && std::filesystem::exists(dirPath)) {
                    m_background = new Texture2D(dirPath.string());
                    m_background->Size = UDim2::fromOffset(window->GetBufferWidth(), window->GetBufferHeight());
                }

                if (chart->m_backgroundBuffer.size() > 0 && m_background == nullptr) {
                    m_background = new Texture2D((uint8_t *)chart->m_backgroundBuffer.data(), chart->m_backgroundBuffer.size());
                    m_background->Size = UDim2::fromOffset(window->GetBufferWidth(), window->GetBufferHeight());
                }

                if (m_background == nullptr) {
                    IsFile = true;
                    auto skinPath = SkinManager::GetInstance()->GetPath();
                    auto noImage = skinPath / "Playing" / "NoImage.png";

                    if (std::filesystem::exists(noImage)) {
                        m_background = new Texture2D(noImage);
                        m_background->Size = UDim2::fromOffset(window->GetBufferWidth(), window->GetBufferHeight());
                    }
                }
            }
        } catch (SDLException &e) {
            MsgBox::Show("FailChart", "Error", "Failed to create texture: " + std::string(e.what()));
            fucked = true;
        }
    }

    if (m_counter > 3.0 && chart != nullptr) {
        if (IsFile) {
            EnvironmentSetup::SetObj("SongBackground", m_background);
        }
        bool is7K = EnvironmentSetup::GetInt("KeyCount") == 7;
        if (!is7K)
        {
            MsgBox::Show("Only7K", "Error", "Only 7K Mode Allowed!", MsgBoxType::OK);
            SceneManager::ChangeScene(GameScene::SONGSELECT);
        }
        else {
            SceneManager::ChangeScene(GameScene::GAMEPLAY);
            EnvironmentSetup::SetInt("FillStart", 1);
        }
    } else {
        if (fucked) {
            std::string songId = EnvironmentSetup::Get("Key");
            if (songId.size() > 0) {
                if (m_counter > 1) {
                    SceneManager::ChangeScene(GameScene::SONGSELECT);
                }
            } else {
                if (MsgBox::GetResult("FailChart") == 4) {
                    SceneManager::GetInstance()->StopGame();
                }
            }
        }
    }
}

void LoadingScene::LoadModifiers() // For File Opened
{
    std::string difficultyValue = Configuration::Load("Gameplay", "Difficulty");
    int setDifficulty = std::stoi(difficultyValue);
    switch (setDifficulty) {
    case 0: // EZ
        EnvironmentSetup::SetInt("Difficulty", 0);
        break;
    case 1: // NM
        EnvironmentSetup::SetInt("Difficulty", 1);
        break;
    case 2: // HD
        EnvironmentSetup::SetInt("Difficulty", 2);
        break;
    }

    std::string selectedMods = Configuration::Load("Gameplay", "Modifiers");
    std::istringstream iss(selectedMods);
    std::string mod;
    while (std::getline(iss, mod, ',')) {
        EnvironmentSetup::SetInt(mod, 1);
    }

    std::string arenaValue = Configuration::Load("Gameplay", "Arena");
    EnvironmentSetup::SetInt("Arena", std::stoi(arenaValue));
}

void LoadingScene::Render(double delta)
{
    if (m_background && is_ready) {
        m_background->Draw();
    }
}

bool LoadingScene::Attach()
{
    SceneManager::DisplayFade(0, [] {});

    EnvironmentSetup::SetInt("FillStart", 0);

    fucked = false;
    is_shown = false;
    is_ready = true;
    m_counter = 0;

    m_background = (Texture2D *)EnvironmentSetup::GetObj("SongBackground");
    dont_dispose = m_background != nullptr;
    return true;
}

bool LoadingScene::Detach()
{
    if (m_background && !dont_dispose && !IsFile) {
        delete m_background;
        m_background = nullptr;
    }

    EnvironmentSetup::SetInt("FillStart", 1);

    return true;
}
