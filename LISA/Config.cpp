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

#include "Config.h"
#include "Debug.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <sstream>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

namespace { // anonymous

const std::string APPS_PATH_KEY_NAME{"appspath"};
const std::string DB_PATH_KEY_NAME{"dbpath"};
const std::string DATA_PATH_KEY_NAME{"datapath"};
const std::string ANNOTATIONS_FILE_KEY_NAME{"annotationsFile"};
const std::string ANNOTATIONS_REGEX_KEY_NAME{"annotationsRegex"};
const std::string DOWNLOAD_RETRY_AFTER_SECS_KEY_NAME{"downloadRetryAfterSeconds"};
const std::string DOWNLOAD_RETRY_MAX_TIMES_KEY_NAME{"downloadRetryMaxTimes"};
const std::string DOWNLOAD_TIMEOUT_SECS_KEY_NAME{"downloadTimeoutSeconds"};

void assureEndsWithSlash(std::string& str)
{
    if ((!str.empty()) && str.back() != '/') {
        str += '/';
    }
}

} // namespace anonymous

Config::Config(const std::string& aConfig)
{
    INFO(" ");

    std::stringstream ss{aConfig};
    boost::property_tree::ptree pt;

    try {
        boost::property_tree::read_json(ss, pt);

        using boost::property_tree::ptree;
        ptree::const_iterator end = pt.end();

        for (auto it = pt.begin(); it != end; ++it) {
            if (it->first == APPS_PATH_KEY_NAME) {
                appsPath = it->second.get_value<std::string>();
                assureEndsWithSlash(appsPath);
                appsTmpPath = appsPath + "tmp/";
            }
            else if (it->first == DB_PATH_KEY_NAME) {
                databasePath = it->second.get_value<std::string>();
                assureEndsWithSlash(databasePath);
            }
            else if (it->first == DATA_PATH_KEY_NAME) {
                appsStoragePath = it->second.get_value<std::string>();
                assureEndsWithSlash(appsStoragePath);
            }
            else if (it->first == ANNOTATIONS_FILE_KEY_NAME) {
                annotationsFile = it->second.get_value<std::string>();
            }
            else if (it->first == ANNOTATIONS_REGEX_KEY_NAME) {
                annotationsRegex = it->second.get_value<std::string>();
            }
            else if (it->first == DOWNLOAD_RETRY_AFTER_SECS_KEY_NAME) {
                downloadRetryAfterSeconds = it->second.get_value<unsigned int>();
            }
            else if (it->first == DOWNLOAD_RETRY_MAX_TIMES_KEY_NAME) {
                downloadRetryMaxTimes = it->second.get_value<unsigned int>();
            }
            else if (it->first == DOWNLOAD_TIMEOUT_SECS_KEY_NAME) {
                downloadTimeoutSeconds = it->second.get_value<unsigned int>();
            }
        }
    }
    catch(std::exception& exc) {
        ERROR("parsing config exception: ", exc.what());
    }
}

const std::string& Config::getDatabasePath() const
{
    return databasePath;
}

const std::string& Config::getAppsTmpPath() const
{
    return appsTmpPath;
}

const std::string& Config::getAppsPath() const
{
    return appsPath;
}

const std::string& Config::getAppsStoragePath() const
{
    return appsStoragePath;
}

const std::string& Config::getAnnotationsFile() const
{
    return annotationsFile;
}

const std::string& Config::getAnnotationsRegex() const
{
    return annotationsRegex;
}

unsigned int Config::getDownloadRetryAfterSeconds() const
{
    return downloadRetryAfterSeconds;
}

unsigned int Config::getDownloadRetryMaxTimes() const
{
    return downloadRetryMaxTimes;
}

unsigned int Config::getDownloadTimeoutSeconds() const
{
    return downloadTimeoutSeconds;
}

std::ostream& operator<<(std::ostream& out, const Config& config)
{
    return out << "[appsPath: " << config.appsPath << " tmpPath: " << config.appsTmpPath << " appStoragePath: "
               << config.appsStoragePath
               << " annotationsFile: " << config.annotationsFile
               << " annotationsRegex: " << config.annotationsRegex
               << " downloadRetryAfterSeconds: " << config.downloadRetryAfterSeconds
               << " downloadRetryMaxTimes: " << config.downloadRetryMaxTimes
               << " downloadTimeoutSeconds: " << config.downloadTimeoutSeconds
            << "]";
};

} // namespace WPEFramework
} // namespace Plugin
} // namespace LISA
