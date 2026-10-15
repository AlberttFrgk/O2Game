#pragma once
#include "Scene.h"
#include "Texture/Texture2D.h"
#include <memory>
#include <string>

struct HitRecord {
    float TimeMs = 0.0f;
    float OffsetMs = 0.0f;
    int   Result = 0;
};

struct HealthRecord {
    float TimeMs = 0.0f;
    float Health = 100.0f;
};

struct ResultScoreData {
    int Score = 0;
    int Cool = 0;
    int Good = 0;
    int Bad = 0;
    int Miss = 0;
    int TotalNotes = 0;

    int JamCombo = 0;
    int MaxJamCombo = 0;
    int Combo = 0;
    int MaxCombo = 0;
    int LNCombo = 0;
    int LNMaxCombo = 0;

    float Accuracy = 0.0f;
    bool Failed = false;

    std::string Grade = "D";
    std::string ClearType = "STAGE CLEAR";
    uint32_t GradeColor = 0xFFFFFFFF;
    uint32_t ClearColor = 0xFFFFFFFF;

    std::vector<float> HitOffsets;
    std::vector<HitRecord> HitRecords;
    std::vector<HealthRecord> HealthRecords;
    float SongDurationMs = 0.0f;
    float SongAudioPositionMs = 0.0f;
    float SongProgress = 1.0f;

    int EarlyCount = 0;
    int LateCount = 0;
    int PerfectCount = 0;
    float MeanHitOffset = 0.0f;
    float HitOffsetStdDev = 0.0f;
};

struct ResultSongData {
    std::string Title = "Unknown Title";
    std::string Artist = "Unknown Artist";
    std::string DiffName = "NM";
    int KeyCount = 7;
    int Level = 0;
    int Category = 0;
    float BPM = 130.0f;
    std::string Modifiers = "None";
};

class ResultScene : public Scene
{
public:
    ResultScene();

    void Render(double delta) override;

    bool Attach() override;
    bool Detach() override;

private:
    bool m_backButton = false;
    bool m_retryButton = false;
    bool m_transitionStarted = false;

    double m_animTime = 0.0;
    bool m_animFinished = false;

    ResultScoreData m_scoreData;
    ResultSongData  m_songData;

    Texture2D* m_background = nullptr;
};