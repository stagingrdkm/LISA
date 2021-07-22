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
#include "DataStorage.h"

namespace WPEFramework {
namespace Plugin {

class SqlDataStorage: public DataStorage {
    public:
        SqlDataStorage(const std::string path): db_path(std::move(path)) {};
        ~SqlDataStorage();

        bool Initialize() override;
    private:
        static sqlite3* sqlite;
        const std::string db_path;

        void Terminate();
        bool InitDB();
        bool OpenConnection();
        bool CreateTables() const;
        bool EnableForeignKeys() const;
        bool ExecuteCommand(const std::string& command) const;
    };

}
}