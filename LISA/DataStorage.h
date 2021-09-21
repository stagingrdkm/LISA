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
#include <stdexcept>
#include <vector>

namespace WPEFramework {
namespace Plugin {
namespace LISA {


class DataStorageError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class DataStorage {
    public:
        virtual ~DataStorage() {}
        virtual void Initialize() = 0;
        virtual std::vector<std::string> GetAppsPaths(const std::string& type, const std::string& id, const std::string& version) = 0;
        virtual std::vector<std::string> GetDataPaths(const std::string& type, const std::string& id) = 0;

        virtual void AddInstalledApp(const std::string& type,
                                     const std::string& id,
                                     const std::string& version,
                                     const std::string& url,
                                     const std::string& appName,
                                     const std::string& category,
                                     const std::string& appPath) = 0;

        virtual bool IsAppInstalled(const std::string& type,
                                    const std::string& id,
                                    const std::string& version) = 0;

        virtual void RemoveInstalledApp(const std::string& type,
                                        const std::string& id,
                                        const std::string& version) = 0;

        virtual void RemoveAppData(const std::string& type,
                                   const std::string& id) = 0;
};

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework

