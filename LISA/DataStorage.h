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
        struct AppDetails
        {
            std::string type;
            std::string id;
            std::string version;
            std::string appName;
            std::string category;
            std::string url;

            AppDetails() {}
            AppDetails(const char *type_, const char *id_, const char *version_, const char *appName_,
                       const char *category_, const char *url_) {
                type = type_ ? type_ : "";
                id = id_ ? id_ : "";
                version = version_ ? version_ : "";
                appName = appName_ ? appName_ : "";
                category = category_ ? category_ : "";
                url = url_ ? url_ : "";
            }
        };

        struct AppMetadata
        {
            AppDetails appDetails;
            std::vector<std::pair<std::string, std::string> > metadata;
        };

        virtual ~DataStorage() {}
        virtual void Initialize() = 0;
        virtual std::vector<std::string> GetAppsPaths(const std::string& type = {},
                                                      const std::string& id = {},
                                                      const std::string& version = {}) = 0;
        virtual std::vector<std::string> GetDataPaths(const std::string& type = {},
                                                      const std::string& id = {}) = 0;
        virtual std::vector<AppDetails> GetAppDetailsList(const std::string& type = {},
                                                          const std::string& id = {},
                                                          const std::string& version = {},
                                                          const std::string& appName = {},
                                                          const std::string& category = {}) = 0;
        virtual std::vector<AppDetails> GetAppDetailsListOuterJoin(const std::string& type = {},
                                                      const std::string& id = {},
                                                      const std::string& version = {},
                                                      const std::string& appName = {},
                                                      const std::string& category = {}) = 0;

        virtual void AddInstalledApp(const std::string& type,
                                     const std::string& id,
                                     const std::string& version,
                                     const std::string& url,
                                     const std::string& appName,
                                     const std::string& category,
                                     const std::string& appPath,
                                     const std::string& appStoragePath) = 0;

        virtual bool IsAppInstalled(const std::string& type,
                                    const std::string& id,
                                    const std::string& version) = 0;

        virtual std::string GetTypeOfApp(const std::string& id) = 0;

        virtual bool IsAppData(const std::string& type,
                               const std::string& id) = 0;

        virtual void RemoveInstalledApp(const std::string& type,
                                        const std::string& id,
                                        const std::string& version) = 0;

        virtual void RemoveAppData(const std::string& type,
                                   const std::string& id) = 0;

        virtual void SetMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key,
                         const std::string& value) = 0;

        virtual void ClearMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key) = 0;

        virtual AppMetadata GetMetadata(const std::string& type,
                                        const std::string& id,
                                        const std::string& version) = 0;

        friend std::ostream& operator<<(std::ostream& out,
                                        const AppDetails& details)
        {
            return out << "[" << details.type << ":" << details.id << ":" << details.version << ":" << details.appName
                       << ":" << details.category << "]";
        }
};

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework

