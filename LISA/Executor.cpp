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
#include "Debug.h"
#include "Downloader.h"
#include "Filesystem.h"
#include "SqlDataStorage.h"

#include <array>
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

std::string generateHandle()
{
    return std::to_string(std::rand());
}

} // namespace anonymous

enum ReturnCodes {
    ERROR_NONE = 0, //Core::ERROR_NONE,
    ERROR_GENERAL = 1,
    ERROR_WRONG_PARAMS = 1001,
    ERROR_TOO_MANY_REQUESTS = 1002,
    ERROR_ALREADY_INSTALLED = 1003,
};

uint32_t Executor::Configure(const std::string& dbPath)
{
    auto result{ERROR_GENERAL};
    try {
        handleDirectories();
        initializeDataBase(dbPath);
        result = ERROR_NONE;
        INFO("configuration done");
    } catch (std::exception& error) {
        ERROR("Unable to configure executor: ", error.what());
        return Core::ERROR_GENERAL;
    }
    return result;
}

uint32_t Executor::Install(const std::string& type,
                           const std::string& id,
                           const std::string& version,
                           const std::string& url,
                           const std::string& appName,
                           const std::string& category,
                           std::string& handle)
{
    INFO("type=", type, " id=", id, " version=", version, " url=", url, " appName=", appName, " cat=", category);

    // TODO what are param requirements?
    if (false /* checkParams() */ ) {
        handle = "WrongParams";
        return ERROR_WRONG_PARAMS;
    }

    if (isAppInstalled(type, id, version)) {
        handle = "AlreadyInstalled";
        return ERROR_ALREADY_INSTALLED;
    }

    LockGuard lock(taskMutex);
    if (isWorkerBusy()) {
        handle = "TooManyRequests";
        return ERROR_TOO_MANY_REQUESTS;
    }

    currentTask.handle = generateHandle();

    executeTask([=] {
        INFO("executing doInstall");
        doInstall(type, id, version, url, appName, category);
    });

    INFO(currentTask, " scheduled ");

    handle = currentTask.handle;
    return ERROR_NONE;
}

uint32_t Executor::Uninstall(const std::string& type,
        const std::string& id,
        const std::string& version,
        const std::string& uninstallType,
        std::string& handle)
{
    INFO("type=", type, " id=", id, " version=", version, " uninstallType=", uninstallType);

    // TODO what are param requirements?
    if (uninstallType != "full" && uninstallType != "upgrade") {
        handle = "WrongParams";
        return ERROR_WRONG_PARAMS;
    }

    if (! isAppInstalled(type, id, version)) {
        handle = "WrongParams";
        return ERROR_WRONG_PARAMS;
    }

    LockGuard lock(taskMutex);
    if (isWorkerBusy()) {
        handle = "TooManyRequests";
        return ERROR_TOO_MANY_REQUESTS;
    }

    currentTask.handle = generateHandle();

    executeTask([=] {
        INFO("executing doUninstall");
        doUninstall(type, id, version, uninstallType);
    });

    return ERROR_NONE;
}

uint32_t Executor::GetProgress(const std::string& handle, std::uint32_t& progress)
{
    LockGuard lock(taskMutex);
    if (handle != currentTask.handle) {
        return ERROR_WRONG_PARAMS;
    }
    progress = currentTask.progress;
    return ERROR_NONE;
}

bool Executor::getStorageParamsValid(const std::string& type,
        const std::string& id,
        const std::string& version) const
{
    // In Stage 1 we support only none parameters or all of them
    return ((!type.empty() && !id.empty() && !version.empty()) || (type.empty() && id.empty() && version.empty()));
}

uint32_t Executor::GetStorageDetails(const std::string& type,
                           const std::string& id,
                           const std::string& version,
                           Filesystem::StorageDetails& details,
                           std::shared_ptr<LISA::DataStorage> storage)
{
    namespace fs = Filesystem;
    if(!getStorageParamsValid(type, id, version)) {
        return ERROR_WRONG_PARAMS;
    }

    if(type.empty()) {
        INFO("Calculating overall usage");
        details.appPath = fs::getAppsDir();
        details.appUsedKB = std::to_string(fs::getDirectorySpace(fs::getAppsDir()) + fs::getDirectorySpace(fs::getAppsTmpDir()));
        details.persistentPath = fs::getAppsStorageDir();
        details.persistentUsedKB = std::to_string(fs::getDirectorySpace(fs::getAppsStorageDir()));
    } else {
        INFO("Calculating usage for: type = ", type, " id = ", id, " version = ", version);
        std::vector<std::string> appsPaths = storage->GetAppsPaths(type, id, version);
        long appUsedKB{};
        // In Stage 1 there will be only one entry here
        for(const auto& i: appsPaths)
        {
            appUsedKB += fs::getDirectorySpace(i);
            details.appPath = i;
        }
        std::vector<std::string> dataPaths = storage->GetDataPaths(type, id);
        long persistentUsedKB{};
        for(const auto& i: dataPaths)
        {
            persistentUsedKB += fs::getDirectorySpace(i);
            details.persistentPath = i;
        }
        details.appUsedKB = std::to_string(appUsedKB);
        details.persistentUsedKB = std::to_string(persistentUsedKB);
    }
    return ERROR_NONE;
}

void Executor::handleDirectories()
{
    Filesystem::createDirectory(Filesystem::getAppsDir() + Filesystem::LISA_EPOCH);
    Filesystem::createDirectory(Filesystem::getAppsStorageDir() + Filesystem::LISA_EPOCH);
    Filesystem::removeAllDirectoriesExcept(Filesystem::getAppsDir(), Filesystem::LISA_EPOCH);
    Filesystem::removeAllDirectoriesExcept(Filesystem::getAppsStorageDir(), Filesystem::LISA_EPOCH);
}

void Executor::initializeDataBase(const std::string& dbPath)
{
    std::string path = dbPath + '/' + Filesystem::LISA_EPOCH;
    Filesystem::ScopedDir dbDir(path);
    dataBase = std::make_unique<LISA::SqlDataStorage>(path);
    dataBase->Initialize();
    dbDir.commit();
    INFO("Database created");
}

bool Executor::isWorkerBusy() const
{
    return ! currentTask.handle.empty();
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
    INFO(task, " started ");

    std::string handle;
    OperationStatus status = OperationStatus::SUCCESS;
    std::string details;

    try {
        task();
        INFO(task, " done");
        status = OperationStatus::SUCCESS;
    }
    catch(std::exception& exc){
        ERROR("exception running ", task, ": ", exc.what());
        status = OperationStatus::FAILED;
        details = exc.what();
    }

    {
        LockGuard lock(taskMutex);
        INFO("scheduled ", currentTask, " done");
        // TODO Check if not cancelled and do not notify with operationStatus
        handle = currentTask.handle;
        currentTask = Task{};
    }

    operationStatusCallback(handle, status, details);
}

bool Executor::isAppInstalled(const std::string& type,
                              const std::string& id,
                              const std::string& version)
{
    auto appInstalled{false};
    try {
        appInstalled = dataBase->IsAppInstalled(type, id, version);
    }
    catch(std::exception& exc) {
        ERROR("error while checking if app installed: ", exc.what());
    }
    return appInstalled;
}

void Executor::doInstall(std::string type,
                         std::string id,
                         std::string version,
                         std::string url,
                         std::string appName,
                         std::string category)
{
    INFO("url=", url, " appName=", appName, " cat=", category);

    // TODO check authentication method

    auto appSubPath = Filesystem::createAppPath(type, id, version);
    INFO("appSubPath: ", appSubPath);

    auto tmpPath = Filesystem::getAppsTmpDir();
    auto tmpDirPath = tmpPath + appSubPath;
    Filesystem::ScopedDir scopedTmpDir{tmpDirPath};

    auto progressListener = [this] (int progress) {
        setProgress(progress, OperationStage::DOWNLOADING);
    };

    Downloader downloader{url, progressListener};

    auto downloadSize = downloader.getContentLength();
    auto tmpFreeSpace = Filesystem::getFreeSpace(tmpDirPath);

    INFO("download size: ", downloadSize, " free tmp space: ", tmpFreeSpace);

    if (downloadSize > tmpFreeSpace) {
        std::string message = std::string{} + "not enough space on " + tmpPath + " (available: "
                + std::to_string(tmpFreeSpace) +", required: " + std::to_string(downloadSize) + ")";
        throw std::runtime_error(message);
    }

    auto tmpFilePath = tmpDirPath + extractFilename(url);

    downloader.get(tmpFilePath);

    const std::string appsPath = Filesystem::getAppsDir() + appSubPath;
    INFO("creating ", appsPath);
    Filesystem::ScopedDir scopedAppDir{appsPath};

    setProgress(0, OperationStage::UNTARING);
    INFO("unpacking ", tmpFilePath, "to ", appsPath);
    Archive::unpack(tmpFilePath, appsPath);

    auto appStoragePath = Filesystem::getAppsStorageDir() + appSubPath;
    INFO("creating storage ", appStoragePath);
    Filesystem::ScopedDir scopedAppStorageDir{appStoragePath};

    setProgress(0, OperationStage::UPDATING_DATABASE);
    dataBase->AddInstalledApp(type, id, version, url, appName, category, appSubPath);

    // everything went fine, mark app directories to not be removed
    scopedAppDir.commit();
    scopedAppStorageDir.commit();

    setProgress(0, OperationStage::FINISHED);

    // TODO invoke maintenace and cleanup

    INFO("finished");
}

void Executor::doUninstall(std::string type, std::string id, std::string version, std::string uninstallType)
{
    INFO("type=", type, " id=", id, " version=", version, " uninstallType=", uninstallType);

    dataBase->RemoveInstalledApp(type, id, version);

    // TODO should "id" be also removed? what if two versions are installed?
    auto appSubPath = Filesystem::createAppPath(type, id, version);
    auto appPath = Filesystem::getAppsDir() + appSubPath;

    INFO("removing ", appPath);
    Filesystem::removeDirectory(appPath);

    if (uninstallType == "full") {
        dataBase->RemoveAppData(type, id);
        auto appStoragePath = Filesystem::getAppsStorageDir() + appSubPath;
        INFO("removing storage directory ", appStoragePath);
        Filesystem::removeDirectory(appStoragePath);
    }

    // TODO invoke maintenace and cleanup

    INFO("finished");
}

void Executor::setProgress(int stagePercent, OperationStage stage)
{
    constexpr auto STAGES = enumToInt(OperationStage::COUNT);
    const std::array<int, STAGES> stageBase = {{0, 90, 95, 100}};
    const std::array<double, STAGES> stageFactor = {{90.0/100, 5.0/100, 5.0/100, 0}};

    int stageIndex = enumToInt(stage);
    int resultPercent = stageBase[stageIndex] + (static_cast<int>(stagePercent * stageFactor[stageIndex]));

    INFO("overall: ", resultPercent, "% from stage: ", stage, " progress: ", stagePercent, "%", " ");

    LockGuard lock{taskMutex};
    currentTask.progress = resultPercent;
}

std::ostream& operator<<(std::ostream& out, const Executor::Task& task)
{
    return out << "task[" << task.handle << "]";
}

std::ostream& operator<<(std::ostream& out, Executor::OperationStage stage)
{
    const std::array<const char*, enumToInt(Executor::OperationStage::COUNT)> stages = {{
            "DOWNLOADING",
            "UNTARING",
            "UPDATING_DATABASE",
            "FINISHED"}};

    return out << stages[enumToInt(stage)];
}

std::ostream& operator<<(std::ostream& out, const Executor::OperationStatus& status)
{
    return out << (status == Executor::OperationStatus::SUCCESS ? "SUCCESS" : "FAILED") ;
}

} // namespace LISA
} // namespace WPEFramework
} // namespace Plugin
