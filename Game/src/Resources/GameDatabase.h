#pragma once
#include <filesystem>
#include <sqlite3.h>
#include <string>
#include <vector>

struct DB_MusicItem
{
    int   Id = -1;
    int   KeyCount = 7;
    float BPM = 240.0f;

    char8_t Title[64];
    char8_t Artist[32];
    char8_t Noter[32];
    char    Hash[3][128];

    int Difficulty[3];
    int MaxNotes[3];

    int CoverOffset;
    int ThumbnailSize;
    int CoverSize;

    int  Category = 0; // 0: O2Jam, 1: osu!mania, 2: BMS
    char FilePath[512] = {};
};

class GameDatabase
{
public:
    static GameDatabase *GetInstance();
    static void          Release();

    void         Insert(DB_MusicItem &item);
    DB_MusicItem Find(int id);
    DB_MusicItem Random();

    std::vector<DB_MusicItem> FindAll(int category = -1);
    std::vector<DB_MusicItem> FindQuery(std::string query, int category = -1);

    std::filesystem::path GetPath();
    int                   GetMusicCount();

    void Reset();

private:
    GameDatabase();
    ~GameDatabase();

    static GameDatabase *m_instance;

    sqlite3 *m_database;
};