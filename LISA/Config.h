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

namespace WPEFramework {
namespace Plugin {
namespace LISA {

class Config
{
public:
    Config() = default;
    Config(const std::string& aConfig);

    const std::string& getDatabasePath() const;
    const std::string& getAppsTmpPath() const;
    const std::string& getAppsPath() const;
    const std::string& getAppsStoragePath() const;
    const std::string& getAnnotationsFile() const;
    const std::string& getAnnotationsRegex() const;
    unsigned int getDownloadRetryAfterSeconds() const;
    unsigned int getDownloadRetryMaxTimes() const;
    unsigned int getDownloadTimeoutSeconds() const;

    friend std::ostream& operator<<(std::ostream& out, const Config& config);

private:
    std::string databasePath{"/mnt/apps/dac/db/"};
    std::string appsPath{"/mnt/apps/dac/images/"};
    std::string appsTmpPath{"/mnt/apps/dac/images/tmp/"};
    std::string appsStoragePath{"/mnt/data/dac/"};
    std::string annotationsFile;
    std::string annotationsRegex;
    unsigned int downloadRetryAfterSeconds{30};
    unsigned int downloadRetryMaxTimes{4};
    unsigned int downloadTimeoutSeconds{15 * 60};
};

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework

