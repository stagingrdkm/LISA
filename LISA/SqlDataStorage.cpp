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
                                         const std::string& appPath)
    {
        auto timeCreated = timeNow();

        InsertIntoApps(type, id, appPath, timeCreated);
        auto appIdx = GetAppIdx(type, id, version);
        InsertIntoInstalledApps(appIdx, version, appName, category, url, appPath, timeCreated);
    }

    bool SqlDataStorage::IsAppInstalled(const std::string& type,
                                        const std::string& id,
                                        const std::string& version)
    {
        int appIdx{INVALID_INDEX};

        std::stringstream query;
        query << "SELECT idx FROM installed_apps";
        query << " WHERE app_idx IN (SELECT idx FROM apps WHERE type == '" << type << "'";
        query << " AND app_id == '" << id << "')";
        query << " AND version == '" << version << "';";

        ExecuteCommand(query.str(), [](void* appIdx,
                                       int columns,
                                       char** columnsTxt,
                                       char** columnName) -> int {

                                           ASSERT(columnsTxt && columnsTxt[0]);
                                           try {
                                               *(static_cast<int*>(appIdx)) = std::stoi(columnsTxt[0]);
                                           }
                                           catch(...) {
                                               // skip silently
                                           }
                                           return 0;
                                       }, reinterpret_cast<void*>(&appIdx));

        return appIdx != INVALID_INDEX;
    }

    void SqlDataStorage::RemoveInstalledApp(const std::string& type,
                                            const std::string& id,
                                            const std::string& version)
    {
        DeleteFromInstalledApps(type, id, version);
    }

    void SqlDataStorage::RemoveAppData(const std::string& type,
                                       const std::string& id)
    {
        DeleteFromApps(type, id);
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
                                    "app_id TEXT NOT NULL,"
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
                                              "FOREIGN KEY(app_idx) REFERENCES apps(idx)"
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
        }
    }

    std::vector<std::string> SqlDataStorage::GetAppsPaths(const std::string& type, const std::string& id, const std::string& version)
    {
        std::stringstream query;
        query << "SELECT app_path FROM installed_apps";
        if(!type.empty()) {
            query << " WHERE app_idx IN (SELECT idx FROM apps WHERE type == '" << type << "'";
            if(!id.empty()) {
                query << " AND app_id == '" << id << "')";
                if(!version.empty()) {
                    query << " AND version == '" << version << "'";
                }
            } else {
                query << ")";
            }
        }
        query << ";";
        return GetPaths(query.str());
    }

    std::vector<std::string> SqlDataStorage::GetDataPaths(const std::string& type, const std::string& id)
    {
        std::stringstream query;
        query << "SELECT data_path FROM apps";
        if(!type.empty()) {
            query << " WHERE type == '" << type << "'";
            if(!id.empty()) {
                query << " AND app_id == '" << id << "'";
            }
        }
        query << ";";
        return GetPaths(query.str());
    }

    std::vector<std::string> SqlDataStorage::GetPaths(const std::string& query) const
    {
        std::vector<std::string> paths;
        ExecuteCommand(query.c_str(), [](void* paths, int columns, char** resp, char**)->int
        {
            for(auto i = 0; i < columns; ++i)
            {
                static_cast<std::vector<std::string>*>(paths)->push_back(resp[i]);
            }
            return 0;
        }, &paths);
        return paths;
    }

    void SqlDataStorage::InsertIntoApps(const std::string& type,
                                        const std::string& id,
                                        const std::string& appPath,
                                        const std::string& timeCreated)
    {
        std::stringstream query;
        query << "INSERT INTO apps VALUES(NULL, ";
        query << "'" << type << "', ";
        query << "'" << id << "', ";
        query << "'" << appPath << "', ";
        query << "'" << timeCreated + "');";

        ExecuteCommand(query.str());
    }

    int SqlDataStorage::GetAppIdx(const std::string& type,
                                  const std::string& id,
                                  const std::string& version)
    {
        int appIdx{INVALID_INDEX};

        std::stringstream query;
        query << "SELECT idx";
        query << " FROM apps WHERE type == '" << type << "' AND app_id == '" << id;
        query << "';";

        ExecuteCommand(query.str(), [](void* appIdx,
                                       int columns,
                                       char** columnsTxt,
                                       char** columnName) -> int {

                                           ASSERT(columnsTxt && columnsTxt[0]);
                                           try {
                                               *(static_cast<int*>(appIdx)) = std::stoi(columnsTxt[0]);
                                           }
                                           catch(...) {
                                               ERROR("error while converting index");
                                           }
                                           return 0;
                                       }, reinterpret_cast<void*>(&appIdx));
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
        assert(appIdx != INVALID_INDEX);

        std::stringstream query;
        query << "INSERT INTO installed_apps VALUES(NULL, ";
        query << std::to_string(appIdx) << ", ";
        query << "'" << version << "', ";
        query << "'" << name << "', ";
        query << "'" << category << "', ";
        query << "'" << url << "', ";
        query << "'" << appPath << "', ";
        query << "'" << timeCreated << "', ";
        query << "NULL, ";
        query << "NULL);";

        ExecuteCommand(query.str());
    }

    void SqlDataStorage::DeleteFromInstalledApps(const std::string& type,
                                                 const std::string& id,
                                                 const std::string& version)
    {
        auto appIdx = GetAppIdx(type, id, version);

        std::stringstream query;
        query << "DELETE FROM installed_apps";
        query << " WHERE idx == " << std::to_string(appIdx);
        query << ";";

        ExecuteCommand(query.str());
    }

    void SqlDataStorage::DeleteFromApps(const std::string& type,
                                        const std::string& id)
    {
        std::stringstream query;
        query << "DELETE FROM apps";
        query << " WHERE type == '" << type << "'";
        query << " AND app_id == '" << id << "'";
        query << ";";

        ExecuteCommand(query.str());
    }

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
