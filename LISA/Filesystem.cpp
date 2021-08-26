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

#include "Filesystem.h"

#include <Module.h>

#include <boost/filesystem.hpp>

namespace WPEFramework {
namespace Plugin {
namespace LISA {
namespace Filesystem {

namespace { // anonymous

//const std::string APPS_DIR = "/mnt/apps/";
//const std::string APPS_TMP_DIR = APPS_DIR + "tmp/";
//const std::string APPS_STORAGE_DIR = "/mnt/apps_storage/";

const std::string APPS_DIR = "/tmp/apps/";
const std::string APPS_TMP_DIR = "/tmp/";
const std::string APPS_STORAGE_DIR = "/tmp/apps_storage/";


void normalizeName(std::string& str)
{
    auto isNotPosixCompatibile = [](const char c) {
        return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '.') || (c == '-')
        || (c == '_'));
    };
    std::replace_if(str.begin(), str.end(), isNotPosixCompatibile, '_');
}

} // namespace anonymous

std::string getAppsTmpDir()
{
    return APPS_TMP_DIR;
}

std::string getAppsDir()
{
    return APPS_DIR;
}

std::string getAppsStorageDir()
{
    return APPS_STORAGE_DIR;
}

std::string createAppSubPath(std::string pathPart)
{
    normalizeName(pathPart);
    return pathPart + '/';
}

bool directoryExists(const std::string& path)
{
    bool dirExists{false};

    try {
        dirExists = boost::filesystem::exists(path);
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " checking if directory " + path + "exists";
        throw FilesystemError(message);
    }

    return dirExists;
}

bool createDirectory(const std::string& path)
{
    bool result{false};

    try {
        result = boost::filesystem::create_directories(path);
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " creating directory " + path;
        throw FilesystemError(message);
    }

    return result;
}

ScopedDir::ScopedDir(const std::string& path)
{
    auto currentPathIndex = path.find_first_of("/");

    while (currentPathIndex != std::string::npos) {
        auto currentSubDir = path.substr(0, currentPathIndex + 1);

        if (!directoryExists(currentSubDir)) {
            dirToRemove = currentSubDir;
            break;
        }
        currentPathIndex = path.find("/", currentPathIndex + 1);
    }

    dirExists = createDirectory(path);

    TRACE_L1("path %s existing dir: %s", path.c_str(), dirToRemove.c_str());
}

ScopedDir::~ScopedDir()
{
    if ((! wasCommited) && (! dirToRemove.empty())) {
        removeDirectory(dirToRemove);
    }
}

void ScopedDir::commit()
{
    wasCommited = true;
}

bool ScopedDir::exists() const
{
    return dirExists;
}

void removeDirectory(const std::string& path)
{
    TRACE_L1("removing directory %s", path.c_str());

    try {
        boost::filesystem::remove_all(path);
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " removing directory " + path;
        throw FilesystemError(message);
    }
}

long getFreeSpace(const std::string& path)
{
    long freeSpace{};

    try {
        auto boostSpace = boost::filesystem::space(path);
        freeSpace = boostSpace.available;
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " reading free space on " + path;
        throw FilesystemError(message);
    }

    return freeSpace;
}

} // namespace Filesystem
} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework



