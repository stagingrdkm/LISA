/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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
#include "SqlDataStorage.h"

#ifndef SQLITE_FILE_HEADER
#define SQLITE_FILE_HEADER "SQLite format 3"
#endif

#define LOGINFO(fmt, ...) do { fprintf(stderr, "INFO [%s:%d] %s: " fmt "\n", strrchr("/" __FILE__, '/') + 1, __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)
#define LOGERR(fmt, ...) do { fprintf(stderr, "ERROR [%s:%d] %s: " fmt "\n", strrchr("/" __FILE__, '/') + 1, __LINE__, __FUNCTION__, ##__VA_ARGS__); fflush(stderr); } while (0)


namespace WPEFramework {
namespace Plugin {

    sqlite3* SqlDataStorage::sqlite = nullptr;

    SqlDataStorage::~SqlDataStorage()
    {
        Terminate();
    }

    void SqlDataStorage::Terminate()
    {
        if(sqlite)
        {
            sqlite3_close(sqlite);
        }
        sqlite = nullptr;
    }

    bool SqlDataStorage::Initialize()
    {
        if(InitDB() == false)
        {
            LOGERR("Initializing database failed!");
            return false;
        }
        return true;
    }

    bool SqlDataStorage::InitDB()
    {
        LOGINFO("Initializing database");
        Terminate();
        return OpenConnection() && CreateTables() && EnableForeignKeys();
    }

    bool SqlDataStorage::OpenConnection()
    {
        LOGINFO("Opening database connection: %s", db_path.c_str());
        bool rc = sqlite3_open(db_path.c_str(), &sqlite);
        if(rc)
        {
            LOGERR("%d - %s", rc, sqlite3_errmsg(sqlite));
            return false;
        }
        return true;
    }

    bool SqlDataStorage::CreateTables() const
    {
        LOGINFO("Creating LISA tables");
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
        LOGINFO("Enabling foreign keys");
        return ExecuteCommand("PRAGMA foreign_keys = ON;");
    }

    bool SqlDataStorage::ExecuteCommand(const std::string& command) const
    {
        char* errmsg;
        int rc = sqlite3_exec(sqlite, command.c_str(), 0, 0, &errmsg);
        if(rc != SQLITE_OK || errmsg)
        {
            if (errmsg)
            {
              LOGERR("%d : %s", rc, errmsg);
              sqlite3_free(errmsg);
            }
            else
            {
              LOGERR("%d", rc);
            }
            return false;
        }
        return true;
    }
}
}
