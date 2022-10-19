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

#pragma once

#include "DataStorage.h"

#include <string>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

using SqlDataStorageError = DataStorageError;

class SqlDataStorage: public DataStorage
{
    public:
        explicit SqlDataStorage(const std::string& path): db_path(path + db_name) {}
        SqlDataStorage(const SqlDataStorage&) = delete;
        SqlDataStorage& operator=(const SqlDataStorage&) = delete;
        ~SqlDataStorage();

        void Initialize() override;
        std::vector<std::string> GetAppsPaths(const std::string& type, const std::string& id, const std::string& version) override;

        std::vector<std::string> GetDataPaths(const std::string& type, const std::string& id) override;

        std::vector<DataStorage::AppDetails> GetAppDetailsList(const std::string& type, const std::string& id, const std::string& version,
                                                  const std::string& appName, const std::string& category) override;

        std::vector<DataStorage::AppDetails> GetAppDetailsListOuterJoin(const std::string& type, const std::string& id, const std::string& version,
                                                                        const std::string& appName, const std::string& category) override;
        void AddInstalledApp(const std::string& type,
                             const std::string& id,
                             const std::string& version,
                             const std::string& url,
                             const std::string& appName,
                             const std::string& category,
                             const std::string& appPath,
                             const std::string& appStoragePath) override;

        bool IsAppInstalled(const std::string& type,
                            const std::string& id,
                            const std::string& version) override;

        std::string GetTypeOfApp(const std::string& id) override;

        bool IsAppData(const std::string& type,
                       const std::string& id) override;

        void RemoveInstalledApp(const std::string& type,
                                const std::string& id,
                                const std::string& version) override;

        void RemoveAppData(const std::string& type,
                           const std::string& id) override;

        void SetMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key,
                         const std::string& value) override;

        void ClearMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key) override;

        DataStorage::AppMetadata GetMetadata(const std::string& type,
                               const std::string& id,
                               const std::string& version) override;

    private:
        static sqlite3* sqlite;
        const std::string db_name = "apps.db";
        const std::string db_path;
        using SqlCallback = int (*)(void*, int, char**, char**);
        constexpr static int INVALID_INDEX = -1;

        void Terminate();
        void InitDB();
        void OpenConnection();
        void CreateTables() const;
        void EnableForeignKeys() const;
        void ExecuteCommand(const std::string& command, SqlCallback callback = nullptr, void* val = nullptr) const;
        void Validate() const;
        std::vector<std::string> GetPaths(sqlite3_stmt* stmt) const;

        void InsertIntoApps(const std::string& type,
                            const std::string& id,
                            const std::string& appPath,
                            const std::string& created);

        int GetAppIdx(const std::string& type,
                      const std::string& id);

        void InsertIntoInstalledApps(int idx,
                                     const std::string& version,
                                     const std::string& name,
                                     const std::string& category,
                                     const std::string& url,
                                     const std::string& appPath,
                                     const std::string& timeCreated);

        void DeleteFromInstalledApps(const std::string& type,
                                     const std::string& id,
                                     const std::string& version);

        void DeleteFromApps(const std::string& type,
                            const std::string& id);

        void ExecuteSqlStep(sqlite3_stmt* stmt);
    };

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
