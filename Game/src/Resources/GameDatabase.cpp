#include "GameDatabase.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string.h>

GameDatabase *GameDatabase::m_instance = nullptr;

static std::mutex g_mutex;
const int         TABLE_VERSION = 5;
const char        LEGACY_DB[2] = { 'D', 'B' };

GameDatabase::GameDatabase()
{
    std::filesystem::path path = std::filesystem::current_path();
    path /= "Game.db";

    if (std::filesystem::exists(path)) {
        std::ifstream fs(path, std::ios::binary);

        // check if legacy database
        char buffer[2];
        fs.read(buffer, 2);

        bool found = memcmp(buffer, LEGACY_DB, 2) == 0;
        fs.close();

        if (found) {
            // legacy database found, delete it
            std::filesystem::remove(path);
        }
    }

    // open with UTF-8 support
    int rc = sqlite3_open_v2((const char *)path.u8string().c_str(), &m_database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc) {
        sqlite3_close(m_database);
        m_database = nullptr;

        throw std::runtime_error("Failed to open database");
    }

    // config creation
    {
        int   result = 0;
        char *error = nullptr;

        result = sqlite3_exec(m_database, "CREATE TABLE IF NOT EXISTS CONFIG(Key VARCHAR(32) PRIMARY KEY, Value VARCHAR(32))", nullptr, nullptr, &error);
        if (result != SQLITE_OK) {
            sqlite3_free(error);
            sqlite3_close(m_database);
            m_database = nullptr;

            throw std::runtime_error("Failed to create table");
        }

        sqlite3_free(error);
    }

    // check database
    {
        const char *key = "Version";
        char        value[32] = {};

        sqlite3_stmt *stmt = nullptr;
        int           result = sqlite3_prepare_v2(m_database, "SELECT Value FROM CONFIG WHERE Key = ?;", -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement");
        }

        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

        result = sqlite3_step(stmt);
        if (result == SQLITE_ROW) {
            strncpy(value, reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0)), 32);
            sqlite3_finalize(stmt);

            // check version
            int version = std::atoi(value);
            if (version < TABLE_VERSION) {
                result = sqlite3_prepare_v2(m_database, "UPDATE CONFIG SET Value = ? WHERE Key = ?", -1, &stmt, nullptr);
                if (result != SQLITE_OK) {
                    throw std::runtime_error("Failed to prepare statement");
                }

                std::string value_str = std::to_string(TABLE_VERSION);
                memset(value, 0, 32);

                strcpy(value, value_str.c_str());

                sqlite3_bind_text(stmt, 1, value, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, key, -1, SQLITE_STATIC);

                result = sqlite3_step(stmt);
                if (result != SQLITE_DONE) {
                    throw std::runtime_error("Failed to update item");
                }

                sqlite3_finalize(stmt);

                // finally drop table
                result = sqlite3_exec(m_database, "DROP TABLE MusicItems;", nullptr, nullptr, nullptr);
                if (result != SQLITE_OK) {
                    throw std::runtime_error("Failed to drop table");
                }
            }
        } else {
            // set Version to TABLE_VERSION
            sqlite3_finalize(stmt);

            result = sqlite3_prepare_v2(m_database, "INSERT INTO CONFIG(Key, Value) VALUES (?, ?);", -1, &stmt, nullptr);
            if (result != SQLITE_OK) {
                throw std::runtime_error("Failed to prepare statement");
            }

            sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);

            char buffer[32] = {};
            sprintf(buffer, "%d", TABLE_VERSION);

            sqlite3_bind_text(stmt, 2, buffer, -1, SQLITE_STATIC);

            result = sqlite3_step(stmt);
            if (result != SQLITE_DONE) {
                throw std::runtime_error("Failed to insert item");
            }

            sqlite3_finalize(stmt);
        }
    }

    // table creation
    {
        int   result = 0;
        char *error = nullptr;

        const char *TABLE_MusicItems = "CREATE TABLE IF NOT EXISTS MusicItems (\n"
                                       "Id INTEGER PRIMARY KEY,"
                                       "KeyCount INTEGER,"
                                       "Title VARCHAR(128),"
                                       "Artist VARCHAR(64),"
                                       "Noter VARCHAR(64),"
                                       "BPM REAL,"
                                       "Hash1 VARCHAR(128),"
                                       "Hash2 VARCHAR(128),"
                                       "Hash3 VARCHAR(128),"
                                       "Difficulty1 INTEGER,"
                                       "Difficulty2 INTEGER,"
                                       "Difficulty3 INTEGER,"
                                       "MaxNotes1 INTEGER,"
                                       "MaxNotes2 INTEGER,"
                                       "MaxNotes3 INTEGER,"
                                       "CoverOffset INTEGER,"
                                       "ThumbnailSize INTEGER,"
                                       "CoverSize INTEGER,"
                                       "Category INTEGER DEFAULT 0,"
                                       "FilePath VARCHAR(512) DEFAULT ''"
                                       ");";

        result = sqlite3_exec(m_database, TABLE_MusicItems, nullptr, nullptr, &error);
        if (result != SQLITE_OK) {
            sqlite3_free(error);
            sqlite3_close(m_database);
            m_database = nullptr;

            throw std::runtime_error("Failed to create table");
        }

        sqlite3_free(error);
        sqlite3_exec(m_database, "CREATE INDEX IF NOT EXISTS idx_music_category ON MusicItems(Category);", nullptr, nullptr, nullptr);
        sqlite3_exec(m_database, "CREATE INDEX IF NOT EXISTS idx_music_title ON MusicItems(Title);", nullptr, nullptr, nullptr);
    }
}

GameDatabase::~GameDatabase()
{
    if (m_database != nullptr) {
        if (sqlite3_close_v2(m_database) != SQLITE_OK) {
            ::printf("Failed to close database\n");
        }

        m_database = nullptr;
    }
}

GameDatabase *GameDatabase::GetInstance()
{
    if (m_instance == nullptr) {
        m_instance = new GameDatabase;
    }

    return m_instance;
}

void GameDatabase::Release()
{
    if (m_instance != nullptr) {
        delete m_instance;
        m_instance = nullptr;
    }
}

std::filesystem::path GameDatabase::GetPath()
{
    std::filesystem::path path = std::filesystem::current_path();
    path /= "Music";

    return path;
}

int GameDatabase::GetMusicCount()
{
    sqlite3_stmt *stmt = nullptr;
    int           result = sqlite3_prepare_v2(m_database, "SELECT COUNT(*) FROM MusicItems;", -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        std::string message = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_ROW) {
        std::string message = "Failed to retreive Item count: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }

    int count = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);

    return count;
}

void GameDatabase::Reset()
{
    const char *TABLE_Reset = "DELETE FROM MusicItems;";

    int result = sqlite3_exec(m_database, TABLE_Reset, nullptr, nullptr, nullptr);
    if (result != SQLITE_OK) {
        std::string message = "Failed to reset table: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }
}

void GameDatabase::Insert(DB_MusicItem &item)
{
    sqlite3_stmt *stmt = nullptr;
    const char   *TABLE_InsertItem = "INSERT INTO MusicItems ("
                                     "Id,"
                                     "KeyCount,"
                                     "Title,"
                                     "Artist,"
                                     "Noter,"
                                     "BPM,"
                                     "Hash1,"
                                     "Hash2,"
                                     "Hash3,"
                                     "Difficulty1,"
                                     "Difficulty2,"
                                     "Difficulty3,"
                                     "MaxNotes1,"
                                     "MaxNotes2,"
                                     "MaxNotes3,"
                                     "CoverOffset,"
                                     "ThumbnailSize,"
                                     "CoverSize,"
                                     "Category,"
                                     "FilePath"
                                     ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    int result = sqlite3_prepare_v2(m_database, TABLE_InsertItem, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        std::string message = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }

    if (item.Id > 0) {
        sqlite3_bind_int(stmt, 1, item.Id);
    } else {
        sqlite3_bind_null(stmt, 1);
    }
    sqlite3_bind_int(stmt, 2, item.KeyCount);

    sqlite3_bind_text(stmt, 3, reinterpret_cast<const char *>(item.Title), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, reinterpret_cast<const char *>(item.Artist), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, reinterpret_cast<const char *>(item.Noter), -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt, 6, item.BPM);

    sqlite3_bind_text(stmt, 7, reinterpret_cast<const char *>(item.Hash[0]), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, reinterpret_cast<const char *>(item.Hash[1]), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, reinterpret_cast<const char *>(item.Hash[2]), -1, SQLITE_STATIC);

    sqlite3_bind_int(stmt, 10, item.Difficulty[0]);
    sqlite3_bind_int(stmt, 11, item.Difficulty[1]);
    sqlite3_bind_int(stmt, 12, item.Difficulty[2]);

    sqlite3_bind_int(stmt, 13, item.MaxNotes[0]);
    sqlite3_bind_int(stmt, 14, item.MaxNotes[1]);
    sqlite3_bind_int(stmt, 15, item.MaxNotes[2]);

    sqlite3_bind_int(stmt, 16, item.CoverOffset);
    sqlite3_bind_int(stmt, 17, item.ThumbnailSize);
    sqlite3_bind_int(stmt, 18, item.CoverSize);

    sqlite3_bind_int(stmt, 19, item.Category);
    sqlite3_bind_text(stmt, 20, item.FilePath, -1, SQLITE_STATIC);

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        std::string message = "Failed to insert item: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }

    if (item.Id <= 0) {
        item.Id = (int)sqlite3_last_insert_rowid(m_database);
    }

    sqlite3_finalize(stmt);
}

DB_MusicItem GameDatabase::Find(int id)
{
    std::lock_guard lock(g_mutex);

    // find from database
    DB_MusicItem item = {};

    // TABLE_FindItems
    sqlite3_stmt *stmt = nullptr;
    const char   *TABLE_FindItems = "SELECT * FROM MusicItems WHERE Id = ? LIMIT 1;";

    int result = sqlite3_prepare_v2(m_database, TABLE_FindItems, -1, &stmt, nullptr);
    if (result != SQLITE_OK) {
        std::string message = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }

    sqlite3_bind_int(stmt, 1, id);

    result = sqlite3_step(stmt);

    if (result == SQLITE_ROW) {
        item.Id = sqlite3_column_int(stmt, 0);
        item.KeyCount = sqlite3_column_int(stmt, 1);

        const char *titleText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        if (titleText) {
            strncpy(reinterpret_cast<char *>(item.Title), titleText, sizeof(item.Title) - 1);
            item.Title[sizeof(item.Title) - 1] = '\0';
        }
        const char *artistText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        if (artistText) {
            strncpy(reinterpret_cast<char *>(item.Artist), artistText, sizeof(item.Artist) - 1);
            item.Artist[sizeof(item.Artist) - 1] = '\0';
        }
        const char *noterText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        if (noterText) {
            strncpy(reinterpret_cast<char *>(item.Noter), noterText, sizeof(item.Noter) - 1);
            item.Noter[sizeof(item.Noter) - 1] = '\0';
        }
        item.BPM = static_cast<float>(sqlite3_column_double(stmt, 5));

        for (int h = 0; h < 3; h++) {
            const char *hashText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6 + h));
            if (hashText) {
                strncpy(reinterpret_cast<char *>(item.Hash[h]), hashText, sizeof(item.Hash[h]) - 1);
                item.Hash[h][sizeof(item.Hash[h]) - 1] = '\0';
            }
        }

        item.Difficulty[0] = sqlite3_column_int(stmt, 9);
        item.Difficulty[1] = sqlite3_column_int(stmt, 10);
        item.Difficulty[2] = sqlite3_column_int(stmt, 11);

        item.MaxNotes[0] = sqlite3_column_int(stmt, 12);
        item.MaxNotes[1] = sqlite3_column_int(stmt, 13);
        item.MaxNotes[2] = sqlite3_column_int(stmt, 14);

        item.CoverOffset = sqlite3_column_int(stmt, 15);
        item.ThumbnailSize = sqlite3_column_int(stmt, 16);
        item.CoverSize = sqlite3_column_int(stmt, 17);

        item.Category = sqlite3_column_int(stmt, 18);
        const char *fp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19));
        if (fp) {
            strncpy(item.FilePath, fp, sizeof(item.FilePath) - 1);
            item.FilePath[sizeof(item.FilePath) - 1] = '\0';
        }
    } else {
        if (result != SQLITE_DONE) {
            sqlite3_finalize(stmt);

            std::string message = "Failed to step statement: " + std::string(sqlite3_errmsg(m_database));
            throw std::runtime_error(message);
        }
    }

    sqlite3_finalize(stmt);

    return item;
}

DB_MusicItem GameDatabase::Random()
{
    std::lock_guard lock(g_mutex);
    DB_MusicItem    item = {};

    sqlite3_stmt *stmt = nullptr;
    const char   *TABLE_RandomItem = "SELECT * FROM MusicItems ORDER BY RANDOM() LIMIT 1;";
    int           result = sqlite3_prepare_v2(m_database, TABLE_RandomItem, -1, &stmt, nullptr);

    if (result != SQLITE_OK) {
        std::string message = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        item.Id = sqlite3_column_int(stmt, 0);
        item.KeyCount = sqlite3_column_int(stmt, 1);

        const char *titleText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        if (titleText) {
            strncpy(reinterpret_cast<char *>(item.Title), titleText, sizeof(item.Title) - 1);
            item.Title[sizeof(item.Title) - 1] = '\0';
        }
        const char *artistText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        if (artistText) {
            strncpy(reinterpret_cast<char *>(item.Artist), artistText, sizeof(item.Artist) - 1);
            item.Artist[sizeof(item.Artist) - 1] = '\0';
        }
        const char *noterText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        if (noterText) {
            strncpy(reinterpret_cast<char *>(item.Noter), noterText, sizeof(item.Noter) - 1);
            item.Noter[sizeof(item.Noter) - 1] = '\0';
        }
        item.BPM = static_cast<float>(sqlite3_column_double(stmt, 5));

        for (int h = 0; h < 3; h++) {
            const char *hashText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6 + h));
            if (hashText) {
                strncpy(reinterpret_cast<char *>(item.Hash[h]), hashText, sizeof(item.Hash[h]) - 1);
                item.Hash[h][sizeof(item.Hash[h]) - 1] = '\0';
            }
        }

        item.Difficulty[0] = sqlite3_column_int(stmt, 9);
        item.Difficulty[1] = sqlite3_column_int(stmt, 10);
        item.Difficulty[2] = sqlite3_column_int(stmt, 11);

        item.MaxNotes[0] = sqlite3_column_int(stmt, 12);
        item.MaxNotes[1] = sqlite3_column_int(stmt, 13);
        item.MaxNotes[2] = sqlite3_column_int(stmt, 14);

        item.CoverOffset = sqlite3_column_int(stmt, 15);
        item.ThumbnailSize = sqlite3_column_int(stmt, 16);
        item.CoverSize = sqlite3_column_int(stmt, 17);

        item.Category = sqlite3_column_int(stmt, 18);
        const char *fp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19));
        if (fp) {
            strncpy(item.FilePath, fp, sizeof(item.FilePath) - 1);
            item.FilePath[sizeof(item.FilePath) - 1] = '\0';
        }
    } else {
        if (result != SQLITE_DONE) {
            sqlite3_finalize(stmt);

            std::string message = "Failed to step statement: " + std::string(sqlite3_errmsg(m_database));
            throw std::runtime_error(message);
        }
    }

    sqlite3_finalize(stmt);

    return item;
}

std::vector<DB_MusicItem> GameDatabase::FindAll(int category)
{
    return FindQuery("", category);
}

std::vector<DB_MusicItem> GameDatabase::FindQuery(std::string query, int category)
{
    std::lock_guard lock(g_mutex);

    sqlite3_stmt *stmt = nullptr;
    int           result = SQLITE_OK;

    std::string toQuery;
    if (query.empty()) {
        const char *TABLE_CategoryItems = "SELECT * FROM MusicItems WHERE (? = -1 OR Category = ?);";
        result = sqlite3_prepare_v2(m_database, TABLE_CategoryItems, -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            std::string message = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_database));
            throw std::runtime_error(message);
        }

        sqlite3_bind_int(stmt, 1, category);
        sqlite3_bind_int(stmt, 2, category);
    } else {
        const char *TABLE_AllItems = "SELECT * FROM MusicItems WHERE (Title LIKE ? OR Artist LIKE ? OR Noter LIKE ? OR Id = ?) AND (? = -1 OR Category = ?);";
        result = sqlite3_prepare_v2(m_database, TABLE_AllItems, -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            std::string message = "Failed to prepare statement: " + std::string(sqlite3_errmsg(m_database));
            throw std::runtime_error(message);
        }

        toQuery = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, toQuery.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, toQuery.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, toQuery.c_str(), -1, SQLITE_TRANSIENT);

        if (std::all_of(query.begin(), query.end(), ::isdigit)) {
            int id = std::stoi(query);
            sqlite3_bind_int(stmt, 4, id);
        } else {
            sqlite3_bind_int(stmt, 4, -1);
        }

        sqlite3_bind_int(stmt, 5, category);
        sqlite3_bind_int(stmt, 6, category);
    }

    int                       size = GetMusicCount();
    std::vector<DB_MusicItem> items;
    items.reserve(size);

    while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
        DB_MusicItem item = {};
        item.Id = sqlite3_column_int(stmt, 0);
        item.KeyCount = sqlite3_column_int(stmt, 1);

        const char *titleText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        if (titleText) {
            strncpy(reinterpret_cast<char *>(item.Title), titleText, sizeof(item.Title) - 1);
            item.Title[sizeof(item.Title) - 1] = '\0';
        }
        const char *artistText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3));
        if (artistText) {
            strncpy(reinterpret_cast<char *>(item.Artist), artistText, sizeof(item.Artist) - 1);
            item.Artist[sizeof(item.Artist) - 1] = '\0';
        }
        const char *noterText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4));
        if (noterText) {
            strncpy(reinterpret_cast<char *>(item.Noter), noterText, sizeof(item.Noter) - 1);
            item.Noter[sizeof(item.Noter) - 1] = '\0';
        }
        item.BPM = static_cast<float>(sqlite3_column_double(stmt, 5));

        for (int h = 0; h < 3; h++) {
            const char *hashText = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 6 + h));
            if (hashText) {
                strncpy(reinterpret_cast<char *>(item.Hash[h]), hashText, sizeof(item.Hash[h]) - 1);
                item.Hash[h][sizeof(item.Hash[h]) - 1] = '\0';
            }
        }

        item.Difficulty[0] = sqlite3_column_int(stmt, 9);
        item.Difficulty[1] = sqlite3_column_int(stmt, 10);
        item.Difficulty[2] = sqlite3_column_int(stmt, 11);

        item.MaxNotes[0] = sqlite3_column_int(stmt, 12);
        item.MaxNotes[1] = sqlite3_column_int(stmt, 13);
        item.MaxNotes[2] = sqlite3_column_int(stmt, 14);

        item.CoverOffset = sqlite3_column_int(stmt, 15);
        item.ThumbnailSize = sqlite3_column_int(stmt, 16);
        item.CoverSize = sqlite3_column_int(stmt, 17);

        item.Category = sqlite3_column_int(stmt, 18);
        const char *fp = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 19));
        if (fp) {
            strncpy(item.FilePath, fp, sizeof(item.FilePath) - 1);
            item.FilePath[sizeof(item.FilePath) - 1] = '\0';
        }

        items.push_back(item);
    }

    sqlite3_finalize(stmt);

    if (result != SQLITE_DONE) {
        std::string message = "Failed to step statement: " + std::string(sqlite3_errmsg(m_database));
        throw std::runtime_error(message);
    }

    return items;
}
