/*
 * MIT License
 *
 * Copyright (c) 2023 Estrol Mendex
 * See the LICENSE file in the root of this project for details.
 */

#pragma once

#include "../Game/Core/Database/GameDatabase.h"
#include "../Game/Core/Drawable/ButtonImage.h"
#include <Screens/Base.h>
#include <UI/Image.h>
#include <UI/Text.h>
#include "SceneList.h"
#include <vector>

class SongSelect : public Screens::Base
{
public:
    SongSelect();
    ~SongSelect();

    void Update(double delta) override;
    void Draw(double delta) override;

    bool Attach() override;
    bool Detach() override;

    static int GetId() { return SceneList::SongSelect; }

private:
    void OnGameSelectMusic(double delta);

    int scene_index = 0;
    int index = -1;
    int page = 0;

    bool  isWait = false;
    bool  isScrolled = false;
    float waitTime = 0;

    float currentSpeed = 2.25;
    float currentRate = 1.0;

    float currentAlpha = 100.0;
    float nextAlpha = 100.0;

    bool is_departing = false;
    bool is_quit = false;
    bool is_update_bgm = false;
    bool imgui_modal_quit_confirm = false;

    bool bPlay = false;
    bool bExitPopup = false;
    bool bOptionPopup = false;
    bool bSelectNewSong = false;
    bool bOpenSongContext = false;
    bool bOpenEditor = false;
    bool bOpenRearrange = false;
    bool bScaleOutput = true;

    std::mutex m_imageLock;

    std::vector<std::string>  m_resolutions;
    std::vector<std::string>  m_fps;
    std::vector<UDim2>        m_songListRect;
    std::vector<ButtonImage>  m_buttons;
    std::vector<DB_MusicItem> m_musicList;

    std::vector<std::filesystem::path> m_tempMusicLists;
    std::filesystem::path              file;
    int                                m_tempMusicIndex;
    float                              m_lastTime;
};