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

#include <fstream>
#include <string.h>
#include <sstream>
#include "Debug.h"
#include "SqlDataStorage.h"

#ifndef SQLITE_FILE_HEADER
#define SQLITE_FILE_HEADER "SQLite format 3"
#endif


namespace WPEFramework {
namespace Plugin {
namespace LISA {

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
        if(InitDB() == false) {
            ERROR("Initializing database failed!");
            throw SqlDataStorageError("Initializing database failed!");
        }
    }

    bool SqlDataStorage::InitDB()
    {
        INFO("Initializing database");
        Terminate();
        return OpenConnection() && CreateTables() && EnableForeignKeys();
    }

    bool SqlDataStorage::OpenConnection()
    {
        INFO("Opening database connection: ", db_path);
        bool rc = sqlite3_open(db_path.c_str(), &sqlite);
        if(rc) {
            ERROR("Error opening connection: ", rc, " - ", sqlite3_errmsg(sqlite));
            return false;
        }
        Validate();
        return true;
    }

    bool SqlDataStorage::CreateTables() const
    {
        INFO("Creating LISA tables");
        bool apps = ExecuteCommand("CREATE TABLE IF NOT EXISTS apps("
                                    "idx INTEGER PRIMARY KEY,"
                                    "type TEXT NOT NULL,"
                                    "app_id TEXT NOT NULL,"
                                    "data_path TEXT,"
                                    "created TEXT NOT NULL"
                                    ");");

        bool installed_apps = ExecuteCommand("CREATE TABLE IF NOT EXISTS installed_apps ("
                                              "idx INTEGER PRIMARY KEY,"
                                              "app_idx INTEGER NOT NULL,"
                                              "version TEXT NOT NULL,"
                                              "name TEXT NOT NULL,"
                                              "category TEXT,"
                                              "url TEXT,"
                                              "app_path TEXT,"
                                              "created INTEGER NOT NULL,"
                                              "resources TEXT,"
                                              "metadata TEXT,"
                                              "FOREIGN KEY(app_idx) REFERENCES apps(idx)"
                                              ");");
        return apps && installed_apps;
    }

    bool SqlDataStorage::EnableForeignKeys() const
    {
        INFO("Enabling foreign keys");
        return ExecuteCommand("PRAGMA foreign_keys = ON;");
    }

    bool SqlDataStorage::ExecuteCommand(const std::string& command, SqlCallback callback, void* val) const
    {
        char* errmsg;
        int rc = sqlite3_exec(sqlite, command.c_str(), callback, val, &errmsg);
        if(rc != SQLITE_OK || errmsg) {
            if (errmsg) {
                ERROR("Error executin command: ", command, " - ", rc, " : ", errmsg);
                sqlite3_free(errmsg);
            } else {
                ERROR("Error executin command: ", command, " - ", rc);
            }
            return false;
        }
        return true;
    }

    void SqlDataStorage::Validate() const
    {
        bool ret = 0;
        int rc = ExecuteCommand("PRAGMA integrity_check;", [](void* ret, int, char** resp, char**)->int
        {
            *static_cast<bool*>(ret) = strcmp(resp[0], "ok");
            return 0;
        }, &ret);
        if(ret | !rc) {
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


} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
