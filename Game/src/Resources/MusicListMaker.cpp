#include "MusicListMaker.h"
#include <string.h>
#include <algorithm>
#include <unordered_set>
#include <Configuration.h>

#include "../Data/Chart.hpp"
#include "../Data/OJN.h"
#include "../Data/osu.hpp"
#include "../Data/bms.hpp"
#include "../Data/Util/Util.hpp"

#include "GameDatabase.h"

std::vector<std::filesystem::path> MusicListMaker::PrepareAll()
{
    std::vector<std::filesystem::path> song_files;
    std::unordered_set<std::string> seen_folders;

    // 1. O2Jam directory (.ojn)
    std::filesystem::path o2jam_dir = Configuration::Load("Music", "O2Jam");
    if (o2jam_dir.empty()) {
        o2jam_dir = Configuration::Load("Music", "Folder");
    }
    if (o2jam_dir.empty()) {
        o2jam_dir = std::filesystem::current_path() / "Music";
    }

    if (std::filesystem::exists(o2jam_dir)) {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(o2jam_dir, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end && !ec;
             it.increment(ec)) {
            if (it->is_regular_file()) {
                std::string ext = it->path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".ojn") {
                    song_files.push_back(it->path());
                }
            }
        }
    }

    // 2. osu!mania directory (.osu)
    std::filesystem::path osu_dir = Configuration::Load("Music", "Osu");
    if (!osu_dir.empty() && std::filesystem::exists(osu_dir)) {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(osu_dir, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end && !ec;
             it.increment(ec)) {
            if (it->is_regular_file()) {
                std::string ext = it->path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".osu") {
                    std::string folderKey = it->path().parent_path().string();
                    if (seen_folders.find(folderKey) == seen_folders.end()) {
                        std::filesystem::path p = it->path();
                        Osu::Beatmap bm(p);
                        if (bm.IsValid() && bm.Mode == 3 && (int)bm.CircleSize >= 4 && (int)bm.CircleSize <= 7) {
                            seen_folders.insert(folderKey);
                            song_files.push_back(it->path());
                        }
                    }
                }
            }
        }
    }

    // 3. BMS directory (.bms, .bme, .bml, .bmsc)
    std::filesystem::path bms_dir = Configuration::Load("Music", "BMS");
    if (!bms_dir.empty() && std::filesystem::exists(bms_dir)) {
        std::error_code ec;
        for (std::filesystem::recursive_directory_iterator it(bms_dir, std::filesystem::directory_options::skip_permission_denied, ec), end;
             it != end && !ec;
             it.increment(ec)) {
            if (it->is_regular_file()) {
                std::string ext = it->path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".bmsc") {
                    std::string folderKey = it->path().parent_path().string();
                    if (seen_folders.find(folderKey) == seen_folders.end()) {
                        std::filesystem::path p = it->path();
                        BMS::BMSFile bms;
                        bms.Load(p);
                        if (bms.IsValid() && bms.KeyCount >= 4 && bms.KeyCount <= 7) {
                            seen_folders.insert(folderKey);
                            song_files.push_back(it->path());
                        }
                    }
                }
            }
        }
    }

    return song_files;
}

std::vector<std::filesystem::path> MusicListMaker::Prepare(std::filesystem::path path)
{
    if (path.empty()) {
        return PrepareAll();
    }

    std::vector<std::filesystem::path> song_files;
    path = std::filesystem::is_symlink(path) ? std::filesystem::read_symlink(path) : path;

    if (!std::filesystem::exists(path)) {
        return song_files;
    }

    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(path, std::filesystem::directory_options::skip_permission_denied, ec), end;
         it != end && !ec;
         it.increment(ec)) {
        if (it->is_regular_file()) {
            std::string ext = it->path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".ojn" || ext == ".osu" || ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".bmsc") {
                song_files.push_back(it->path());
            }
        }
    }

    return song_files;
}

void MusicListMaker::Insert(std::filesystem::path song_file)
{
    auto db = GameDatabase::GetInstance();
    std::string ext = song_file.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".ojn") {
        O2::OJN ojn;
        ojn.Load(song_file, false);

        if (ojn.IsValid()) {
            Chart chart(ojn, 0);
            if (chart.m_keyCount < 4 || chart.m_keyCount > 7) {
                return;
            }

            DB_MusicItem item = {};
            item.Id = ojn.Header.songid;
            item.Category = 0; // O2Jam
            item.KeyCount = chart.m_keyCount;
            strncpy(item.FilePath, song_file.string().c_str(), sizeof(item.FilePath) - 1);

            auto title = EnsureUtf8((const char *)ojn.Header.title, sizeof(ojn.Header.title));
            auto noter = EnsureUtf8((const char *)ojn.Header.noter, sizeof(ojn.Header.noter));
            auto artist = EnsureUtf8((const char *)ojn.Header.artist, sizeof(ojn.Header.artist));

            item.CoverOffset = ojn.Header.data_offset[3];
            item.CoverSize = ojn.Header.cover_size;
            item.ThumbnailSize = ojn.Header.bmp_size;
            item.BPM = ojn.Header.bpm;

            memcpy(item.Title, title.c_str(), std::clamp((int)title.size(), 0, (int)(sizeof(item.Title) - 1)));
            memcpy(item.Noter, noter.c_str(), std::clamp((int)noter.size(), 0, (int)(sizeof(item.Noter) - 1)));
            memcpy(item.Artist, artist.c_str(), std::clamp((int)artist.size(), 0, (int)(sizeof(item.Artist) - 1)));

            for (int i = 0; i < (int)ojn.Difficulties.size() && i < 3; i++) {
                Chart diffChart(ojn, i);

                memset(item.Hash[i], 0, 128);
                memcpy(item.Hash[i], diffChart.MD5Hash.c_str(), 128);
                item.MaxNotes[i] = ojn.Header.note_count[i];
                item.Difficulty[i] = ojn.Header.level[i];
            }

            db->Insert(item);
        }
    } else if (ext == ".osu") {
        Osu::Beatmap beatmap(song_file);
        if (beatmap.IsValid() && beatmap.Mode == 3) {
            int keyCount = (int)beatmap.CircleSize;
            if (keyCount >= 4 && keyCount <= 7) {
                DB_MusicItem item = {};
                item.Id = 0; // Auto-increment in SQLite
                item.Category = 1; // osu!mania
                item.KeyCount = keyCount;
                strncpy(item.FilePath, song_file.string().c_str(), sizeof(item.FilePath) - 1);

                std::string title = !beatmap.TitleUnicode.empty() ? beatmap.TitleUnicode : beatmap.Title;
                std::string artist = !beatmap.ArtistUnicode.empty() ? beatmap.ArtistUnicode : beatmap.Artist;
                std::string noter = beatmap.Creator;

                strncpy((char *)item.Title, title.c_str(), sizeof(item.Title) - 1);
                strncpy((char *)item.Artist, artist.c_str(), sizeof(item.Artist) - 1);
                strncpy((char *)item.Noter, noter.c_str(), sizeof(item.Noter) - 1);

                int totalNotes = 0;
                for (const auto &ho : beatmap.HitObjects) {
                    totalNotes += (ho.Type == 128) ? 2 : 1;
                }
                item.MaxNotes[0] = totalNotes;
                item.MaxNotes[1] = totalNotes;
                item.MaxNotes[2] = totalNotes;

                float bpm = 130.0f;
                for (const auto &tp : beatmap.TimingPoints) {
                    if (tp.Inherited != 0 && tp.BeatLength > 0) {
                        bpm = 60000.0f / tp.BeatLength;
                        break;
                    }
                }
                item.BPM = bpm;
                item.Difficulty[0] = (int)beatmap.OverallDifficulty;

                db->Insert(item);
            }
        }
    } else if (ext == ".bms" || ext == ".bme" || ext == ".bml" || ext == ".bmsc") {
        BMS::BMSFile bms;
        bms.Load(song_file);
        if (bms.IsValid() && bms.KeyCount >= 4 && bms.KeyCount <= 7) {
            DB_MusicItem item = {};
            item.Id = 0; // Auto-increment in SQLite
            item.Category = 2; // BMS
            item.KeyCount = bms.KeyCount;
            strncpy(item.FilePath, song_file.string().c_str(), sizeof(item.FilePath) - 1);

            strncpy((char *)item.Title, bms.Title.c_str(), sizeof(item.Title) - 1);
            strncpy((char *)item.Artist, bms.Artist.c_str(), sizeof(item.Artist) - 1);
            strncpy((char *)item.Noter, bms.SubArtist.empty() ? "-" : bms.SubArtist.c_str(), sizeof(item.Noter) - 1);

            int totalNotes = 0;
            for (const auto &note : bms.Notes) {
                totalNotes += (note.EndTime != -1) ? 2 : 1;
            }
            item.MaxNotes[0] = totalNotes;
            item.MaxNotes[1] = totalNotes;
            item.MaxNotes[2] = totalNotes;
            item.BPM = bms.BPM;
            item.Difficulty[0] = bms.Level;

            db->Insert(item);
        }
    }
}