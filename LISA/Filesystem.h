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
namespace Filesystem {

class FilesystemError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

struct StorageDetails
{
    std::string appPath{};
    std::string appQuota{};
    std::string appUsedKB{};
    std::string persistentPath{};
    std::string persistentQuota{};
    std::string persistentUsedKB{};
};

const std::string LISA_EPOCH = "0";

bool isAcceptableFilePath(const std::string& pathPart);
std::string createAppSubPath(std::string pathPart);

template<typename... Args>
std::string createAppSubPath(std::string pathPart, Args... otherParts)
{
    return createAppSubPath(pathPart) + createAppSubPath(otherParts...);
}

template<typename... Args>
std::string createAppPath(Args... pathParts)
{
    return LISA_EPOCH + '/' + createAppSubPath(pathParts...);
}

bool directoryExists(const std::string& path);
bool createDirectory(const std::string& path);
bool createDirectory(const std::string& path, int gid, bool writeable);
void removeDirectory(const std::string& path);
void removeAllDirectoriesExcept(const std::string& path, const std::string& except);
std::vector<std::string> getSubdirectories(const std::string& path);
void setPermission(const std::string& path, int uid, int gid, bool isdir, bool writeable);
void setPermissionsRecursively(const std::string& path, int gid, bool writeable);
bool isEmpty(const std::string& path);

/**
 * Helper class for temporary directories and rolling back changes.
 * Upon destruction will remove created directory if 'commit' wasn't called.
 */
class ScopedDir
{
public:
    ScopedDir(const std::string& path);

    ScopedDir(const ScopedDir& other) = delete;
    ScopedDir& operator=(const ScopedDir& other) = delete;

    ~ScopedDir();
    void commit();
    bool exists() const;
private:
    bool wasCommited{false};
    bool dirExists{false};
    std::string dirToRemove{};
};

unsigned long long getFreeSpace(const std::string& path);
unsigned long long getDirectorySpace(const std::string& path);

} // namespace Filesystem
} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
