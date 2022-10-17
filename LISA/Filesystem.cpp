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

#include "Filesystem.h"
#include "Debug.h"

#include <boost/filesystem.hpp>
#include <unistd.h>

namespace WPEFramework {
namespace Plugin {
namespace LISA {
namespace Filesystem {

namespace { // anonymous

void normalizeName(std::string& str)
{
    auto isNotPosixCompatibile = [](const char c) {
        return !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '.') || (c == '-')
        || (c == '_'));
    };
    std::replace_if(str.begin(), str.end(), isNotPosixCompatibile, '_');
}

} // namespace anonymous

bool isAcceptableFilePath(const std::string& pathPart)
{
    auto normalized = pathPart;
    normalizeName(normalized);
    return pathPart == normalized;
}

std::string createAppSubPath(std::string pathPart)
{
    // normalize not needed because of previous isAcceptableFilePath() check
    return pathPart + '/';
}

bool directoryExists(const std::string& path)
{
    bool dirExists{false};

    try {
        dirExists = boost::filesystem::exists(path);
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + error.what() + " checking if directory exists";
        throw FilesystemError(message);
    }

    return dirExists;
}

bool createDirectory(const std::string& path)
{
    bool result{false};
    INFO("creating directory ", path);
    try {
        result = boost::filesystem::create_directories(path);
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " creating directory";
        throw FilesystemError(message);
    }

    return result;
}

bool createDirectory(const std::string& path, int gid, bool writeable)
{
    bool result{false};
    INFO("creating directory ", path);
    try {
        boost::filesystem::path fullpath(path);
        boost::filesystem::path subpath;
        auto it = fullpath.begin();
        while(it != fullpath.end()) {
            subpath /= (*it);
            if (!boost::filesystem::exists(subpath)) {
                INFO("creating subdir ", subpath);
                result = boost::filesystem::create_directory(subpath);
                if (gid >= 0) {
                    setPermission(subpath.string(), getuid(), gid, true, writeable);
                }
            }
            it++;
        }
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " creating directory";
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

    INFO("path ", path, " existing dir: ", dirToRemove);
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
    INFO("removing directory ", path);

    try {
        boost::filesystem::remove_all(path);
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " removing directory " + path;
        throw FilesystemError(message);
    }
}

void removeAllDirectoriesExcept(const std::string& path, const std::string& except)
{
    INFO("removing directories ", path, " except ", except);

    try {
        boost::filesystem::directory_iterator end_itr;
        for(boost::filesystem::directory_iterator itr(path); itr != end_itr; ++itr)
        {
           if(itr->path().filename().compare(except)) {
               removeDirectory(itr->path().string());
           }
        }
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " removing directories " + path;
        throw FilesystemError(message);
    }
}

std::vector<std::string> getSubdirectories(const std::string& path)
{
    INFO("path: ", path);

    std::vector<std::string> result;
    try {
        namespace bfs = boost::filesystem;
        bfs::directory_iterator endItr;
        for(bfs::directory_iterator itr(path); itr != endItr; ++itr)
        {
           if(bfs::is_directory(*itr)) {
               result.emplace_back(itr->path().filename().string());
           }
        }
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " removing directories " + path;
        throw FilesystemError(message);
    }
    return result;
}

void setPermission(const std::string& path, int uid, int gid, bool isdir, bool group_writeable)
{
    if (chown(path.c_str(), uid, gid)) {
        ERROR("Could not change owner of ", path);
    }

    try {
        auto perms = boost::filesystem::owner_read | boost::filesystem::owner_write | boost::filesystem::group_read;
        if (isdir) {
            perms |= boost::filesystem::owner_exe;
            perms |= boost::filesystem::group_exe;
        }
        if (group_writeable) {
            perms |= boost::filesystem::group_write;
        }
        //INFO("Changing permissions for ", path);
        boost::filesystem::permissions(path, perms);
    }
    catch(boost::filesystem::filesystem_error& error) {
        ERROR("Could not set permissions on ", path, " error: ", error.what());
    }
}

void setPermissionsRecursively(const std::string& path, int gid, bool writeable)
{
    INFO("setPermissionsRecursively: ", path, " ", gid);

    int uid = getuid();
    setPermission(path, uid, gid, true, writeable);
    try {
        namespace bfs = boost::filesystem;
        bfs::recursive_directory_iterator endItr;
        for(bfs::recursive_directory_iterator itr(path); itr != endItr; ++itr)
        {
           setPermission(itr->path().string(), uid, gid, bfs::is_directory(*itr), writeable);
        }
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " setting permissions " + path;
        throw FilesystemError(message);
    }
}

bool isEmpty(const std::string& path)
{
    INFO("path: ", path);

    auto result{false};
    try {
        result = boost::filesystem::is_empty(path);
    }
    catch(boost::filesystem::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " while checking if " + path + " is empty";
        throw FilesystemError(message);
    }
    return result;
}

unsigned long long getFreeSpace(const std::string& path)
{
    unsigned long long freeSpace{};

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

unsigned long long getDirectorySpace(const std::string& path)
{
    uintmax_t space{};
    namespace bf = boost::filesystem;
    try {
        if(directoryExists(path)) {
            for(bf::recursive_directory_iterator it(path); it != bf::recursive_directory_iterator(); ++it)
            {
                if(bf::exists(*it) && !bf::is_directory(*it) && !bf::is_symlink(*it)) {
                    space += bf::file_size(*it);
                }
            }
        }
    }
    catch(bf::filesystem_error& error) {
        std::string message = std::string{} + "error " + error.what() + " reading directory space on " + path;
        throw FilesystemError(message);
    }
    return (unsigned long long)space;
}

} // namespace Filesystem
} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework



