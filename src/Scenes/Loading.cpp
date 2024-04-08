/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#include "Loading.h"

#include "SceneList.h"
#include <Screens/ScreenManager.h>

#include <Exceptions/EstException.h>
#include <Graphics/NativeWindow.h>
#include <Graphics/Renderer.h>
#include <Logs.h>

#include <Misc/Lodepng.h>

#include <Configuration.h>

#include <algorithm>

#include "../Game/Env.h"

#include "../Game/Data/bms.hpp"
#include "../Game/Data/chart.hpp"
#include "../Game/Data/ojn.h"
#include "../Game/Data/osu.hpp"

#include "../Game/Core/Audio/SampleManager.h"

#include "../Game/Core/Database/GameDatabase.h"

#include "../Game/MsgBoxEx.h"
#include <UI/Text.h>

#include "../Game/Core/Networking/GameCoordinator.h"
#include "Gameplay.h"

Loading::Loading()
{
    m_counter = 0.0f;
}

Loading::~Loading()
{
}

void Loading::Draw(double delta)
{
}

void Loading::Update(double delta)
{
    if (is_ready || fucked) {
        m_counter += delta;
    }

    int  songId = Env::GetInt("Key");
    int  diffIndex = Env::GetInt("Difficulty");
    bool IsO2Jam = false;
    bool IsFile = false;
    bool IsThirdParty = false;

    Chart *chart = reinterpret_cast<Chart *>(Env::GetPointer("Chart"));
    if (!chart || (songId != -1 && chart->GetO2JamId() != songId)) {
        if (chart) {
            delete chart;
        }

        if (!fucked) {
            std::filesystem::path file;

            if (songId != -1) {
                file = GameDatabase::Get()->GetPath();
                file /= "o2ma" + std::to_string(songId) + ".ojn";
            } else {
                file = Env::GetPath("FILE");
                IsFile = true;

                auto autoplay = Env::GetInt("ParameterAutoplay");
                auto rate = Env::GetFloat("ParameterRate");

                Env::SetInt("Autoplay", autoplay);
                Env::SetFloat("SongRate", rate);
            }

            const char *bmsfile[] = { ".bms", ".bme", ".bml", ".bmsc" };
            const char *ojnfile = ".ojn";

            if (file.extension() == bmsfile[0] || file.extension() == bmsfile[1] || file.extension() == bmsfile[2] || file.extension() == bmsfile[3]) {
                BMS::BMSFile beatmap;
                beatmap.Load(file);

                if (!beatmap.IsValid()) {
                    MsgBox::InShow("FailChart", "Error", "Failed to BMS chart!", MsgBox::Type::Ok);
                    fucked = true;
                    return;
                }

                chart = new Chart(beatmap);
            } else if (file.extension() == ojnfile) {
                O2::OJN o2jamFile;
                o2jamFile.Load(file);

                if (!o2jamFile.IsValid()) {
                    std::string msg = "Failed to load OJN: " + ("o2ma" + std::to_string(songId) + ".ojn");

                    MsgBox::InShow("FailChart", "Error", msg.c_str(), MsgBox::Type::Ok);
                    fucked = true;
                    return;
                }

                chart = new Chart(o2jamFile, diffIndex);

                IsO2Jam = true;
            } else {
                Osu::Beatmap beatmap(file);

                if (!beatmap.IsValid()) {
                    MsgBox::InShow("FailChart", "Error", "Failed to load osu beatmap!", MsgBox::Type::Ok);
                    fucked = true;
                    return;
                }

                chart = new Chart(beatmap);
            }

            Env::SetPointer("Chart", chart);
        }
    } else {
        if (songId != -1) {
            IsO2Jam = chart->GetO2JamId() == songId;
        }
    }

    std::filesystem::path dirPath = chart->m_beatmapDirectory;
    dirPath /= chart->m_backgroundFile;

    if (IsO2Jam && !IsFile) {
        auto item = GameDatabase::Get()->Find(songId);

        bool hashFound = strcmp(item.Hash[diffIndex], chart->MD5Hash.c_str()) == 0;
        if (!hashFound) {
            MsgBox::InShow("FailChart", "Error", "Invalid map identifier, please refresh music list using F5 key!", MsgBox::Type::Ok);
            fucked = true;
        }
    }

    if (!is_audio_loaded) {
        is_audio_loaded = true;

        double m_rate = Env::GetFloat("SongRate");
        m_rate = std::clamp(m_rate, 0.05, 2.0);

        bool IsPitch = Configuration::GetBool("Game", "AudioPitch");
        SampleManager::SetRate(m_rate);

        auto tr = std::thread([=] {
            SampleManager::Load(chart, false);
        });

        tr.detach();
    }

    if (!fucked) {
        try {
            if (!m_Background) {
                if (chart->m_backgroundFile.size() > 0 && std::filesystem::exists(dirPath)) {
                    m_Background = std::make_shared<Image>(dirPath);
                }

                if (chart->m_backgroundBuffer.size() > 0 && m_Background == nullptr) {
                    m_Background = std::make_shared<Image>(
                        chart->m_backgroundBuffer.data(),
                        chart->m_backgroundBuffer.size());
                }
            }

            is_ready = true;
        } catch (const Exceptions::EstException &e) {
            Logs::Puts("[Loading] texture loading fail: %s", e.what());
            fucked = true;
        }
    }

    if (is_ready && m_Background) {
        m_Background->Draw();
    }

    if (SampleManager::IsLoaded() && !fucked) {
        Screens::Manager::Get()->Set<Gameplay>();
    }
}

bool Loading::Attach()
{
    m_counter = 0.0f;
    Env::SetFloat("ParameterRate", 1.5f);

    Env::SetInt("Key", -1);
    Env::SetInt("Difficulty", 2);
    Env::SetPath("FILE", "F:\\test\\o2ma3467.ojn");

    return true;
}

bool Loading::Detach()
{
    m_Background.reset();

    return true;
}