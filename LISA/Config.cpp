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

std::ostream& operator<<(std::ostream& out, const Config& config)
{
    return out << "[appsPath: " << config.appsPath << " tmpPath: " << config.appsTmpPath << " appStoragePath: "
               << config.appsStoragePath << "]";
};

} // namespace WPEFramework
} // namespace Plugin
} // namespace LISA
