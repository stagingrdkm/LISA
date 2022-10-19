/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2021 Liberty Global Service B.V.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Debug.h"
#include "SqlDataStorage.h"

#include <fstream>
#include <string.h>
#include <sstream>

#ifndef SQLITE_FILE_HEADER
#define SQLITE_FILE_HEADER "SQLite format 3"
#endif

namespace WPEFramework {
namespace Plugin {
namespace LISA {

namespace { // anonymous
    struct SqliteDeleter
    {
        void operator()(void *ptr)
        {
            if (ptr) {
                sqlite3_free(ptr);
            }
        }
    };
    using SqlUniqueString = std::unique_ptr<char, SqliteDeleter>;

    std::string timeNow()
    {
        const auto now = std::chrono::system_clock::now();
        std::time_t todayTime = std::chrono::system_clock::to_time_t(now);
        return std::ctime(&todayTime);
    }
} // namespace anonymous

    sqlite3* SqlDataStorage::sqlite = nullptr;

    SqlDataStorage::~SqlDataStorage()
    {
        Terminate();
    }

    void SqlDataStorage::Terminate()
    {
        if(sqlite) {
            sqlite3_close(sqlite);
        }
        sqlite = nullptr;
    }

    void SqlDataStorage::Initialize()
    {
        InitDB();
    }

    void SqlDataStorage::AddInstalledApp(const std::string& type,
                                         const std::string& id,
                                         const std::string& version,
                                         const std::string& url,
                                         const std::string& appName,
                                         const std::string& category,
                                         const std::string& appPath,
                                         const std::string& appStoragePath)
    {
        auto timeCreated = timeNow();

        int appIdx;
        try {
            appIdx = GetAppIdx(type, id);
        } catch (const SqlDataStorageError& ex) {
            InsertIntoApps(type, id, appStoragePath, timeCreated);
            appIdx = GetAppIdx(type, id);
        }
        InsertIntoInstalledApps(appIdx, version, appName, category, url, appPath, timeCreated);
    }

    bool SqlDataStorage::IsAppInstalled(const std::string& type,
                                        const std::string& id,
                                        const std::string& version)
    {
        INFO(" ");
        std::string query = "SELECT idx FROM installed_apps WHERE app_idx IN (SELECT idx FROM apps WHERE (?1 IS NULL OR type = ?1) AND app_id = ?2 AND version = ?3);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.empty() ? nullptr : type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.empty() ? nullptr : id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.empty() ? nullptr : version.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_ROW) {
            return true;
        } else if (rc == SQLITE_DONE) {
            return false;
        } else {
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
    }

    std::string SqlDataStorage::GetTypeOfApp(const std::string& id)
    {
        INFO(" ");
        std::string type{};
        std::string query = "SELECT type FROM apps WHERE app_id ==  $1;";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
        auto col1 = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
        type = (col1 ? col1 : "");
        sqlite3_finalize(stmt);
        return type;
    }

    bool SqlDataStorage::IsAppData(const std::string& type,
                                   const std::string& id)
    {
        INFO(" ");
        std::string query = "SELECT idx FROM apps WHERE (?1 IS NULL OR type = ?1) AND (?2 IS NULL OR app_id = ?2)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.empty() ? nullptr : type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.empty() ? nullptr : id.c_str(), -1, SQLITE_TRANSIENT);
        int rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (rc == SQLITE_ROW) {
            return true;
        } else if (rc == SQLITE_DONE) {
            return false;
        } else {
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
    }

    void SqlDataStorage::RemoveInstalledApp(const std::string& type,
                                            const std::string& id,
                                            const std::string& version)
    {
        ClearMetadata(type, id, version, "");
        DeleteFromInstalledApps(type, id, version);
    }

    void SqlDataStorage::RemoveAppData(const std::string& type,
                                       const std::string& id)
    {
        DeleteFromApps(type, id);
    }

    void SqlDataStorage::SetMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key,
                         const std::string& value)
    {
        std::string query = "INSERT OR REPLACE INTO metadata(app_idx, meta_key, meta_value) "
                        "VALUES("
                        "(SELECT installed_apps.idx FROM installed_apps INNER JOIN apps ON apps.idx = installed_apps.app_idx WHERE type = ?1 AND app_id = ?2 AND version = ?3),"
                        "?4,"
                        "?5);";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, value.c_str(), -1, SQLITE_TRANSIENT);
        ExecuteSqlStep(stmt);
        sqlite3_finalize(stmt);
    }

    // key can be empty to clear all metadata of an app
    void SqlDataStorage::ClearMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key)
    {
        std::string query = "DELETE FROM metadata "
                       "WHERE metadata.idx IN ("
                       "SELECT metadata.idx FROM metadata "
                       "INNER JOIN installed_apps ON installed_apps.idx = metadata.app_idx "
                       "INNER JOIN apps ON apps.idx = installed_apps.app_idx "
                       "WHERE type = ?1 AND app_id = ?2 AND version = ?3 AND (?4 IS NULL OR meta_key = ?4));";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, key.empty()? nullptr: key.c_str(), -1, SQLITE_TRANSIENT);
        ExecuteSqlStep(stmt);
        sqlite3_finalize(stmt);
    }

    DataStorage::AppMetadata SqlDataStorage::GetMetadata(const std::string& type,
                               const std::string& id,
                               const std::string& version)
    {
        INFO(" ");

        sqlite3_stmt* stmt;
        std::string appDetailsQuery =
            "SELECT type, app_id, version, name, category, url FROM installed_apps "
            "INNER JOIN apps ON apps.idx = installed_apps.app_idx "
            "WHERE type = ?1 AND app_id = ?2 AND version = ?3";

        sqlite3_prepare_v2(sqlite, appDetailsQuery.c_str(), appDetailsQuery.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
        DataStorage::AppDetails appDetails{
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5))};

        sqlite3_finalize(stmt);

        std::string metadataQuery = "SELECT meta_key, meta_value FROM metadata "
                       "INNER JOIN installed_apps ON installed_apps.idx = metadata.app_idx "
                       "INNER JOIN apps ON apps.idx = installed_apps.app_idx "
                       "WHERE type = ?1 AND app_id = ?2 AND version = ?3";

        sqlite3_prepare_v2(sqlite, metadataQuery.c_str(), metadataQuery.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.c_str(), -1, SQLITE_TRANSIENT);

        std::vector<std::pair<std::string, std::string> > metadata;
        int rc{};
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            std::pair<std::string, std::string> keyValue{
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))
            };
            metadata.push_back(keyValue);
        }
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }

        sqlite3_finalize(stmt);

        return AppMetadata{appDetails, metadata};
    }

    void SqlDataStorage::InitDB()
    {
        INFO("Initializing database");
        Terminate();
        OpenConnection();
        Validate();
        CreateTables();
        EnableForeignKeys();
    }

    void SqlDataStorage::OpenConnection()
    {
        INFO("Opening database connection: ", db_path);
        bool rc = sqlite3_open(db_path.c_str(), &sqlite);
        if (rc) {
            auto msg = std::string{"Error opening connection: "} + std::to_string(rc) + " - " + sqlite3_errmsg(sqlite);
            throw SqlDataStorageError(msg);
        }
    }

    void SqlDataStorage::CreateTables() const
    {
        INFO("Creating LISA tables");
        ExecuteCommand("CREATE TABLE IF NOT EXISTS apps("
                                    "idx INTEGER PRIMARY KEY,"
                                    "type TEXT NOT NULL,"
                                    "app_id TEXT UNIQUE NOT NULL,"
                                    "data_path TEXT,"
                                    "created TEXT NOT NULL"
                                    ");");

        ExecuteCommand("CREATE TABLE IF NOT EXISTS installed_apps ("
                                              "idx INTEGER PRIMARY KEY,"
                                              "app_idx INTEGER NOT NULL,"
                                              "version TEXT NOT NULL,"
                                              "name TEXT NOT NULL,"
                                              "category TEXT,"
                                              "url TEXT,"
                                              "app_path TEXT,"
                                              "created TEXT NOT NULL,"
                                              "resources TEXT,"
                                              "metadata TEXT,"
                                              "FOREIGN KEY(app_idx) REFERENCES apps(idx),"
                                              "UNIQUE(app_idx, version)"
                                              ");");

        ExecuteCommand("CREATE TABLE IF NOT EXISTS metadata("
	                        "idx INTEGER PRIMARY KEY,"
                            "app_idx TEXT NOT NULL,"
                            "meta_key TEXT NOT NULL,"
                            "meta_value TEXT NOT NULL,"
                            "FOREIGN KEY(app_idx) REFERENCES installed_apps(idx),"
                            "UNIQUE(app_idx, meta_key)"
                            ");");
    }

    void SqlDataStorage::EnableForeignKeys() const
    {
        INFO("Enabling foreign keys");
        ExecuteCommand("PRAGMA foreign_keys = ON;");
    }

    void SqlDataStorage::ExecuteCommand(const std::string& command, SqlCallback callback, void* val) const
    {
        char* rawErrorMsg{};
        int rc = sqlite3_exec(sqlite, command.c_str(), callback, val, &rawErrorMsg);

        SqlUniqueString errorMsg{rawErrorMsg};
        if(rc != SQLITE_OK || errorMsg)
        {
            auto msg = std::string{"error "} + errorMsg.get() + " while executing " + command;
            throw SqlDataStorageError(msg);
        }
    }

    void SqlDataStorage::Validate() const
    {
        bool integrityCheckFailed{true};
        try {
            ExecuteCommand("PRAGMA integrity_check;", [](void* integrityCheckFailed, int, char** resp, char**)->int
            {
                *static_cast<bool*>(integrityCheckFailed) = strcmp(resp[0], "ok");
                return 0;
            }, &integrityCheckFailed);
        }
        catch(SqlDataStorageError& exc) {
            ERROR("error ", exc.what());
            integrityCheckFailed = true;
        }

        if (integrityCheckFailed) {
            ERROR("database integrity check failed, dropping tables");
            ExecuteCommand("DROP TABLE apps;");
            ExecuteCommand("DROP TABLE installed_apps;");
            ExecuteCommand("DROP TABLE metadata;");
        }
    }

    std::vector<std::string> SqlDataStorage::GetAppsPaths(const std::string& type, const std::string& id, const std::string& version)
    {
        INFO(" ");
        std::string query = "SELECT app_path FROM installed_apps WHERE app_idx IN (SELECT idx FROM apps WHERE (?1 IS NULL OR type = ?1) AND (?2 IS NULL OR app_id = ?2)) AND (?3 IS NULL OR version = ?3)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.empty() ? nullptr : type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.empty() ? nullptr : id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.empty() ? nullptr : version.c_str(), -1, SQLITE_TRANSIENT);
        auto paths = GetPaths(stmt);
        sqlite3_finalize(stmt);
        return paths;
    }

    std::vector<std::string> SqlDataStorage::GetDataPaths(const std::string& type, const std::string& id)
    {
        INFO(" ");
        std::string query = "SELECT data_path FROM apps WHERE (?1 IS NULL OR type = ?1) AND (?2 IS NULL OR app_id = ?2)";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.empty() ? nullptr : type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.empty() ? nullptr : id.c_str(), -1, SQLITE_TRANSIENT);
        auto paths = GetPaths(stmt);
        sqlite3_finalize(stmt);
        return paths;
    }

    std::vector<std::string> SqlDataStorage::GetPaths(sqlite3_stmt* stmt) const
    {
        INFO(" ");
        std::vector<std::string> paths;
        int rc{};
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            paths.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        if (rc != SQLITE_DONE) {
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
        return paths;
    }

    std::vector<DataStorage::AppDetails> SqlDataStorage::GetAppDetailsList(const std::string& type, const std::string& id, const std::string& version,
                                                              const std::string& appName, const std::string& category)
    {
        INFO(" ");
        std::string query = "SELECT A.type,A.app_id,IA.version,IA.name,IA.category,IA.url FROM installed_apps IA, apps A WHERE (IA.app_idx == A.idx) AND (?1 IS NULL OR A.type = ?1) AND (?2 IS NULL OR app_id = ?2) "
                       "AND (?3 IS NULL OR version = ?3) AND (?4 IS NULL OR name = ?4) AND (?5 IS NULL OR category = ?5);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, type.empty() ? nullptr : type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.empty() ? nullptr : id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.empty() ? nullptr : version.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, appName.empty() ? nullptr : appName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, category.empty() ? nullptr : category.c_str(), -1, SQLITE_TRANSIENT);
        int rc{};
        std::vector<AppDetails> appsList;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {

            appsList.push_back(AppDetails{reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                          reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
                          reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5))});
        }
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
        sqlite3_finalize(stmt);
        return appsList;
    }

    // version, appName, category will by empty when no apps_installed record found for a type+id
    std::vector<DataStorage::AppDetails> SqlDataStorage::GetAppDetailsListOuterJoin(const std::string& type, const std::string& id, const std::string& version,
                                                                           const std::string& appName, const std::string& category)
    {
        INFO(" ");
        std::string query = "SELECT type, app_id, version, name, category, url FROM apps LEFT OUTER JOIN installed_apps ON installed_apps.app_idx = apps.idx WHERE (?1 IS NULL OR type = ?1) AND (?2 IS NULL OR app_id = ?2) "
                            "AND (?3 IS NULL OR version = ?3) AND (?4 IS NULL OR name = ?4) AND (?5 IS NULL OR category = ?5);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);
        sqlite3_bind_text(stmt, 1, type.empty() ? nullptr : type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.empty() ? nullptr : id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, version.empty() ? nullptr : version.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, appName.empty() ? nullptr : appName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, category.empty() ? nullptr : category.c_str(), -1, SQLITE_TRANSIENT);
        int rc{};
        std::vector<AppDetails> appsList;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {

            appsList.push_back(AppDetails{reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
                                          reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
                                          reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)), reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5))});
        }
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
        sqlite3_finalize(stmt);
        return appsList;
    }
    void SqlDataStorage::InsertIntoApps(const std::string& type,
                                        const std::string& id,
                                        const std::string& appPath,
                                        const std::string& timeCreated)
    {
        INFO(" ");
        std::string query = "INSERT INTO apps VALUES(NULL, $1, $2, $3, $4);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, appPath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, timeCreated.c_str(), -1, SQLITE_TRANSIENT);
        ExecuteSqlStep(stmt);
        sqlite3_finalize(stmt);
    }

    int SqlDataStorage::GetAppIdx(const std::string& type,
                                  const std::string& id)
    {
        INFO(" ");
        int appIdx{INVALID_INDEX};
        std::string query = "SELECT idx FROM apps WHERE type == $1 AND app_id ==  $2;";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
        appIdx = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return appIdx;
    }

    void SqlDataStorage::InsertIntoInstalledApps(int appIdx,
                                             const std::string& version,
                                             const std::string& name,
                                             const std::string& category,
                                             const std::string& url,
                                             const std::string& appPath,
                                             const std::string& timeCreated)
    {
        INFO(" ");
        assert(appIdx != INVALID_INDEX);
        std::string query = "INSERT INTO installed_apps VALUES(NULL, $1, $2, $3, $4, $5, $6, $7, NULL, NULL);";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, std::to_string(appIdx).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, category.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, url.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, appPath.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, timeCreated.c_str(), -1, SQLITE_TRANSIENT);
        ExecuteSqlStep(stmt);
        sqlite3_finalize(stmt);
    }

    void SqlDataStorage::DeleteFromInstalledApps(const std::string& type,
                                                 const std::string& id,
                                                 const std::string& version)
    {
        INFO(" ");
        auto appIdx = GetAppIdx(type, id);

        std::string query = "DELETE FROM installed_apps WHERE app_idx == $1 AND version == $2;";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, std::to_string(appIdx).c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, version.c_str(), -1, SQLITE_TRANSIENT);
        ExecuteSqlStep(stmt);
        sqlite3_finalize(stmt);
    }

    void SqlDataStorage::DeleteFromApps(const std::string& type,
                                        const std::string& id)
    {
        INFO(" ");
        std::string query = "DELETE FROM apps WHERE type == $1 AND app_id == $2;";
        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);

        sqlite3_bind_text(stmt, 1, type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_TRANSIENT);
        ExecuteSqlStep(stmt);
        sqlite3_finalize(stmt);
    }

    void SqlDataStorage::ExecuteSqlStep(sqlite3_stmt* stmt)
    {
        INFO(" ");
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw SqlDataStorageError(std::string{"sqlite error: "} + sqlite3_errmsg(sqlite));
        }
    }
} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
