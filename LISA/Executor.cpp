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

#include "Executor.h"

#include "Archive.h"
#include "Downloader.h"
#include "Filesystem.h"

#include "Module.h"

#include <cassert>
#include <random>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

namespace // anonymous
{

std::string extractFilename(const std::string& uri)
{
    std::size_t found = uri.find_last_of('/');
    return uri.substr(found+1);
}

bool isAppInstalled(const std::string& type,
                    const std::string& id,
                    const std::string& version)
{
    // TODO check database
    // TODO temporary check by looking for app dir

    // TODO does other app version count?
    auto appSubPath = Filesystem::createAppPath(type, id, version);

    const std::string appDir = Filesystem::getAppsDir() + appSubPath;

    auto result{false};
    try {
        result = Filesystem::directoryExists(appDir);
        TRACE_L1("directory %s %s found", appDir.c_str(), (result ? "" : "not"));
    }
    catch(std::exception& exc) {
        TRACE_L1("%s", exc.what());
    }

    TRACE_L1("assuming app %s installed", (result ? "" : "not"));
    return result;
}

std::string generateHandle()
{
    return std::to_string(std::rand());
}

} // namespace anonymous

enum ReturnCodes {
    ERROR_NONE = 0, //Core::ERROR_NONE,
    ERROR_WRONG_PARAMS = 1001,
    ERROR_TOO_MANY_REQUESTS = 1002,
    ERROR_ALREADY_INSTALLED = 1003,
};

uint32_t Executor::Install(const std::string& type,
                           const std::string& id,
                           const std::string& version,
                           const std::string& url,
                           const std::string& appName,
                           const std::string& category,
                           std::string& handle)
{
    TRACE_L1("type=%s id=%s version=%s url=%s appName=%s cat=%s", type.c_str(), id.c_str(), version.c_str(),
        url.c_str(), appName.c_str(), category.c_str());

    // TODO what are param requirements?
    if (false /* checkParams() */ ) {
        handle = "WrongParams";
        return ERROR_WRONG_PARAMS;
    }

    if (isAppInstalled(type, id, version)) {
        handle = "AlreadyInstalled";
        return ERROR_ALREADY_INSTALLED;
    }

    LockGuard lock(workerMutex);
    if (isWorkerBusy()) {
        handle = "TooManyRequests";
        return ERROR_TOO_MANY_REQUESTS;
    }

    currentHandle = generateHandle();

    executeTask([=] {
        TRACE_L1("executing doInstall");
        doInstall(type, id, version, url, appName, category);
    });

    TRACE_L1("task scheduled, handle %s", currentHandle.c_str());

    handle = currentHandle;
    return ERROR_NONE;
}

uint32_t Executor::Uninstall(const std::string& type,
        const std::string& id,
        const std::string& version,
        const std::string& uninstallType,
        std::string& handle)
{
    TRACE_L1("type=%s id=%s version=%s uninstallType=%s", type.c_str(), id.c_str(), version.c_str(),
        uninstallType.c_str());

    // TODO what are param requirements?
    if (uninstallType != "full" && uninstallType != "upgrade") {
        handle = "WrongParams";
        return ERROR_WRONG_PARAMS;
    }

    if (! isAppInstalled(type, id, version)) {
        handle = "WrongParams";
        return ERROR_WRONG_PARAMS;
    }

    LockGuard lock(workerMutex);
    if (isWorkerBusy()) {
        handle = "TooManyRequests";
        return ERROR_TOO_MANY_REQUESTS;
    }

    currentHandle = generateHandle();

    executeTask([=] {
        TRACE_L1("executing doUninstall");
        doUninstall(type, id, version, uninstallType);
    });

    return ERROR_NONE;
}

bool Executor::isWorkerBusy() const
{
    return ! currentHandle.empty();
}

void Executor::executeTask(std::function<void()> task)
{
    assert(task);

    std::thread worker{[=]
    {
        taskRunner(std::move(task));
    }};
    worker.detach();
}

void Executor::taskRunner(std::function<void()> task)
{
    TRACE_L1("task started %p", task);
    try {
        task();
        // TODO send status "Done"
        TRACE_L1("task %p done", task);
    }
    catch(std::exception& exc){
        TRACE_L1("exception running task %p: %s", task, exc.what());
        // TODO send status "Failed"
    }
    catch(...){
        TRACE_L1("exception ...  running task %p: ", task);
        // TODO send status "Failed"
    }


    LockGuard lock(workerMutex);
    TRACE_L1("scheduled task done, handle %s", currentHandle.c_str());
    currentHandle.clear();
}

void Executor::doInstall(std::string type,
                         std::string id,
                         std::string version,
                         std::string url,
                         std::string appName,
                         std::string category)
{
    TRACE_L1("url=%s appName=%s cat=%s  %p", url.c_str(), appName.c_str(), category.c_str(), this);

    // TODO check authentication method

    Downloader downloader{url};

    auto downloadSize = downloader.getContentLength();

    auto tmpPath = Filesystem::getAppsTmpDir();
    auto tmpFreeSpace = Filesystem::getFreeSpace(tmpPath);

    TRACE_L1("download size: %ld tmp(%s) space: %ld", downloadSize, tmpPath.c_str(), tmpFreeSpace);

    if (downloadSize > tmpFreeSpace) {
        std::string message = std::string{} + "not enough space on " + tmpPath + " (available: "
                + std::to_string(tmpFreeSpace) +", required: " + std::to_string(downloadSize) + ")";
        throw std::runtime_error(message);
    }

    auto appSubPath = Filesystem::createAppPath(type, id, version);
    TRACE_L1("appSubPath (normalized): %s", appSubPath.c_str());

    auto tmpDir = tmpPath + appSubPath;
    Filesystem::ScopedDir scopedTmpDir{tmpDir};

    auto tmpFilePath = tmpDir + extractFilename(url);
    Filesystem::File tmpFile{tmpFilePath};

    downloader.get(tmpFile);

    const std::string appsPath = Filesystem::getAppsDir() + appSubPath;
    TRACE_L1("creating %s ", appsPath.c_str());
    Filesystem::ScopedDir scopedAppDir{appsPath};

    TRACE_L1("unpacking %s to %s", tmpFilePath.c_str(), appsPath.c_str());
    Archive::unpack(tmpFilePath, appsPath);

    auto appStoragePath = Filesystem::getAppsStorageDir() + appSubPath;
    TRACE_L1("creating storage %s ", appsPath.c_str());
    Filesystem::ScopedDir scopedAppStorageDir{appStoragePath};

    // everything went fine, mark app directories to not be removed
    scopedAppDir.commit();
    scopedAppStorageDir.commit();

    // TODO add entry to database

    // TODO invoke maintenace and cleanup
    // TODO notify status

    TRACE_L1("finished, notify status");
}

void Executor::doUninstall(std::string type, std::string id, std::string version, std::string uninstallType)
{
    TRACE_L1("type=%s idName=%s version=%s uninstallType=%s", type.c_str(), id.c_str(), version.c_str(),
        uninstallType.c_str());

    // TODO remove from database

    // TODO should "id" be also removed? what if two versions are installed?
    auto appSubPath = Filesystem::createAppPath(type, id, version);

    auto appPath = Filesystem::getAppsDir() + appSubPath;
    TRACE_L1("removing %s ", appPath.c_str());
    Filesystem::removeDirectory(appPath);

    if (uninstallType == "full") {
        auto appStoragePath = Filesystem::getAppsStorageDir() + appSubPath;
        TRACE_L1("removing storage directory %s ", appStoragePath.c_str());
        Filesystem::removeDirectory(appStoragePath);
    }

    // TODO invoke maintenace and cleanup
    // TODO notify status

    TRACE_L1("finished, notify status");
}

} // namespace LISA
} // namespace WPEFramework
} // namespace Plugin
