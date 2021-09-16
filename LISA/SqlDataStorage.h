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

#pragma once

#include <string>
#include <sqlite3.h>
#include <stdexcept>
#include <vector>
#include <string>
#include "DataStorage.h"

namespace WPEFramework {
namespace Plugin {
namespace LISA {

class SqlDataStorageError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class SqlDataStorage: public DataStorage {
    public:
        explicit SqlDataStorage(const std::string& path): db_path(path + db_name) {}
        ~SqlDataStorage();

        void Initialize() override;
        std::vector<std::string> GetAppsPaths(const std::string& type, const std::string& id, const std::string& version) override;
        std::vector<std::string> GetDataPaths(const std::string& type, const std::string& id) override;
    private:
        static sqlite3* sqlite;
        const std::string db_name = "/apps.db";
        const std::string db_path;
        using SqlCallback = int (*)(void*, int, char**, char**);

        void Terminate();
        bool InitDB();
        bool OpenConnection();
        bool CreateTables() const;
        bool EnableForeignKeys() const;
        bool ExecuteCommand(const std::string& command, SqlCallback callback = nullptr, void* val = nullptr) const;
        void Validate() const;
        std::vector<std::string> GetPaths(const std::string& query) const;
    };

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
