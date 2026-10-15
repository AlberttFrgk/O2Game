#pragma once
#include <vector>

#include "Scene.h"
#include "Texture/Text.h"
#include "Texture/Texture2D.h"

#include "../Engine/BGMPreview.hpp"
#include "../Engine/Button.hpp"
#include "../Engine/SkinConfig.hpp"

#include "../Resources/GameDatabase.h"

struct MouseState;

struct DifficultyOption
{
    std::string           Name;
    int                   KeyCount = 7;
    float                 Rating = 0.0f;
    int                   TotalNotes = 0;
    std::filesystem::path FilePath;
};

class SongSelectScene : public Scene
{
public:
    SongSelectScene();
    ~SongSelectScene() override;

    void Render(double delta) override;
    void Update(double delta) override;
    void Input(double delta) override;
    void OnMouseDown(const MouseState &state) override;

    bool Attach() override;
    bool Detach() override;

protected:
    void OnGameLoadMusic(double delta);
    void OnGameSelectMusic(double delta);

private:
    void SaveConfiguration();
    void SaveModifiers();
    void LoadChartImage();
    void ParseExternalChart(std::filesystem::path file);
    void LoadDifficultyOptions(const DB_MusicItem &item);
    void SelectDifficultyOption(int index);

    int scene_index = 0;
    int index = -1;
    int page = 0;
    int m_selectedCategory = -1;
    int m_sortMode = 0;       // 0: Default, 1: Level/Difficulty, 2: Name
    bool m_sortAscending = true; // true: Small -> Big (A->Z), false: Big -> Small (Z->A)
    void SortSongs();

    bool  isWait = false;
    bool  isScrolled = false;
    float waitTime = 0;

    int  currentDifficulty;
    int  difficulty;

    float currentSpeed = 2.25;
    float currentRate = 1.0;

    float currentAlpha = 100.0;
    float nextAlpha = 100.0;

    bool is_departing = false;
    bool is_quit = false;
    bool is_update_bgm = false;
    bool imgui_modal_quit_confirm = false;

    bool bPlay = false;
    bool bOpenFile = false;
    bool bOptionPopup = false;
    bool bSelectNewSong = false;
    bool bOpenSongContext = false;
    bool bOpenEditor = false;
    bool bOpenRearrange = false;
    bool bScaleOutput = true;
    bool bQuit = false;

    char lanePos[8] = {};

    std::unique_ptr<Texture2D> m_background;
    std::unique_ptr<Texture2D> m_songBackground;

    std::unique_ptr<BGMPreview> m_bgm;

    std::mutex m_imageLock;

    std::vector<std::string>  m_resolutions;
    std::vector<std::string>  m_fps;
    std::vector<std::string>  m_arenas;
    std::vector<UDim2>        m_songListRect;
    std::vector<Button>       m_buttons;
    std::vector<DB_MusicItem> m_musicList;

    SkinConfig                         m_config;
    std::vector<std::filesystem::path> m_tempMusicLists;
    std::filesystem::path              file;
    int                                m_tempMusicIndex;
    float                              m_lastTime;

    std::filesystem::path              m_externalFilePath;
    DB_MusicItem                       m_externalItem;

    std::vector<DifficultyOption>      m_difficultyOptions;
    int                                m_selectedDiffIndex = 0;
};