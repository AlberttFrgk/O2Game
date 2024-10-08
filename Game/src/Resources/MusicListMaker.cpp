#include "MusicListMaker.h"
#include <string.h>

#include "../Data/Chart.hpp"
#include "../Data/OJN.h"
#include "../Data/Util/Util.hpp"

#include "GameDatabase.h"

std::vector<std::filesystem::path> MusicListMaker::Prepare(std::filesystem::path path)
{
    std::vector<std::filesystem::path> song_files;

    path = std::filesystem::is_symlink(path) ? std::filesystem::read_symlink(path) : path;

    for (const auto &dir_entry : std::filesystem::directory_iterator(path)) {
        if (dir_entry.is_regular_file()) {
            std::string fileName = dir_entry.path().filename().string();
            if (fileName.starts_with("o2ma") && fileName.ends_with(".ojn")) {
                song_files.push_back(dir_entry.path());
            }
        }
    }

    std::sort(song_files.begin(), song_files.end(), [](auto &path1, auto &path2) {
        std::string file1 = path1.filename().string(), file2 = path2.filename().string();

        // remove .ojn from file1 and file2 then remove o2ma from both
        file1 = file1.substr(0, file1.find_first_of("."));
        file2 = file2.substr(0, file2.find_first_of("."));
        file1 = file1.substr(4, file1.size());
        file2 = file2.substr(4, file2.size());

        int id1 = std::atoi(file1.c_str());
        int id2 = std::atoi(file2.c_str());

        return id1 < id2;
    });

    return song_files;
}

void MusicListMaker::Insert(std::filesystem::path song_file)
{
    auto db = GameDatabase::GetInstance();

    O2::OJN ojn;
    ojn.Load(song_file, false);

    if (ojn.IsValid()) {
        DB_MusicItem item = {};
        item.Id = ojn.Header.songid;

        auto title = CodepageToUtf8((const char *)ojn.Header.title, sizeof(ojn.Header.title), "euc-kr");
        auto noter = CodepageToUtf8((const char *)ojn.Header.noter, sizeof(ojn.Header.noter), "euc-kr");
        auto artist = CodepageToUtf8((const char *)ojn.Header.artist, sizeof(ojn.Header.artist), "euc-kr");

        item.CoverOffset = ojn.Header.data_offset[3];
        item.CoverSize = ojn.Header.cover_size;
        item.ThumbnailSize = ojn.Header.bmp_size;
        item.BPM = ojn.Header.bpm;

        memcpy(item.Title, title.c_str(), std::clamp((int)title.size(), 0, (int)(sizeof(item.Title) - 1)));
        memcpy(item.Noter, noter.c_str(), std::clamp((int)noter.size(), 0, (int)(sizeof(item.Noter) - 1)));
        memcpy(item.Artist, artist.c_str(), std::clamp((int)artist.size(), 0, (int)(sizeof(item.Artist) - 1)));

        for (int i = 0; i < ojn.Difficulties.size(); i++) {
            Chart chart(ojn, i);

            memset(item.Hash[i], 0, 128);
            memcpy(item.Hash[i], chart.MD5Hash.c_str(), 128);
            item.MaxNotes[i] = ojn.Header.note_count[i];
            item.Difficulty[i] = ojn.Header.level[i];
            item.KeyCount = chart.m_keyCount;
        }

        db->Insert(item);
    }
}