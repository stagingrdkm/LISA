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

#include "Executor.h"

#include "Archive.h"
#include "Config.h"
#include "Debug.h"
#include "Downloader.h"
#include "Filesystem.h"
#include "SqlDataStorage.h"
#include "AuthModule/Auth.h"

#include <array>
#include <cassert>
#include <random>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

extern "C" AuthMethod getAuthenticationMethod(const char* appType, const char* id, const char* url);

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


// TODO some enhancements
// - replace type,id,version triplet with AppId - make it less error prone (named arguments?)
// - create AppConfig that will take Config and encapsulate path creation - at present it's
//   easy easy to make mistake, similar code is different places
struct AppId
{
    std::string type;
    std::string id;
    std::string version;
};

std::ostream& operator<<(std::ostream& out, const AppId& app)
{
    return out << "app[" << app.type << ":" << app.id << ":" << app.version << "]";
}

std::vector<AppId> scanDirectories(const std::string& appsPath, bool scanDataStorage)
{
    std::vector<AppId> apps;
    std::string currentPath;

    auto appsPaths = Filesystem::getSubdirectories(appsPath);
    for (auto& typePath : appsPaths) {

        currentPath = appsPath + typePath + '/';
        if (Filesystem::isEmpty(currentPath)) {
            INFO("empty dir: ", currentPath, " removing");
            Filesystem::removeDirectory(currentPath);
            continue;
        }

        AppId app{};
        app.type = typePath;

        auto idSubPaths = Filesystem::getSubdirectories(currentPath);
        for (auto& idSubPath : idSubPaths) {

            currentPath = appsPath + typePath + '/' + idSubPath + '/';
            if (Filesystem::isEmpty(currentPath)) {
                INFO("empty dir: ", currentPath, " removing");
                Filesystem::removeDirectory(currentPath);
                continue;
            }

            AppId appId = app;
            appId.id = idSubPath;

            if (scanDataStorage) {
                apps.emplace_back(appId);
            } else {
                auto verSubPaths = Filesystem::getSubdirectories(currentPath);
                for (auto& verSubPath : verSubPaths) {

                    currentPath = appsPath + typePath + '/' + idSubPath + '/' + verSubPath + '/';
                    if (Filesystem::isEmpty(currentPath)) {
                        INFO("empty dir: ", currentPath, " removing");
                        Filesystem::removeDirectory(currentPath);
                        continue;
                    }

                    AppId appVer = appId;
                    appVer.version = verSubPath;

                    apps.emplace_back(appVer);
                }
            }
        }
    }
    return apps;
}

} // namespace anonymous

enum ReturnCodes {
    ERROR_NONE = 0, //Core::ERROR_NONE,
    ERROR_GENERAL = 1,
    ERROR_WRONG_PARAMS = 1001,
    ERROR_TOO_MANY_REQUESTS = 1002,
    ERROR_ALREADY_INSTALLED = 1003,
};

uint32_t Executor::Configure(const std::string& configString)
{
    INFO("config: '", configString, "'");
    config = Config{configString};

    auto result{Core::ERROR_NONE};
    try {
        handleDirectories();
        initializeDataBase(config.getDatabasePath());
        doMaintenance();
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

    if (type.empty() || id.empty() || version.empty()) {
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

    handle = currentTask.handle;
    return ERROR_NONE;
}

uint32_t Executor::GetProgress(const std::string& handle, std::uint32_t& progress)
{
    LockGuard lock(taskMutex);
    if (isCurrentHandle(handle)) {
        progress = currentTask.progress;
        return ERROR_NONE;
    } else {
        return ERROR_WRONG_PARAMS;
    }
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
                           Filesystem::StorageDetails& details)
{
    namespace fs = Filesystem;
    if(!getStorageParamsValid(type, id, version)) {
        return ERROR_WRONG_PARAMS;
    }
    try {
        if(type.empty()) {
            INFO("Calculating overall usage");
            details.appPath = config.getAppsPath();
            details.appUsedKB = std::to_string((fs::getDirectorySpace(config.getAppsPath()) + fs::getDirectorySpace(config.getAppsTmpPath())) / 1024);
            details.persistentPath = config.getAppsStoragePath();
            details.persistentUsedKB = std::to_string(fs::getDirectorySpace(config.getAppsStoragePath()) / 1024);
        } else {
            INFO("Calculating usage for: type = ", type, " id = ", id, " version = ", version);
            std::vector<std::string> appsPaths = dataBase->GetAppsPaths(type, id, version);
            unsigned long long appUsedKB{};
            // In Stage 1 there will be only one entry here
            for(const auto& i: appsPaths)
            {
                details.appPath = config.getAppsPath() + i;
                appUsedKB += fs::getDirectorySpace(details.appPath);
            }
            std::vector<std::string> dataPaths = dataBase->GetDataPaths(type, id);
            unsigned long long persistentUsedKB{};
            for(const auto& i: dataPaths)
            {
                details.persistentPath = config.getAppsStoragePath() + i;
                persistentUsedKB += fs::getDirectorySpace(details.persistentPath);
            }
            details.appUsedKB = std::to_string(appUsedKB / 1024);
            details.persistentUsedKB = std::to_string(persistentUsedKB / 1024);
        }
    } catch (std::exception& error) {
        ERROR("Unable to retrive storage details: ", error.what());
        return Core::ERROR_GENERAL;
    }
    return ERROR_NONE;
}

uint32_t Executor::GetAppDetailsList(const std::string& type,
                          const std::string& id,
                          const std::string& version,
                          const std::string& appName,
                          const std::string& category,
                          std::vector<DataStorage::AppDetails>& appsDetailsList) const
{
    try {
        appsDetailsList = dataBase->GetAppDetailsList(type, id, version, appName, category);
    } catch (std::exception& error) {
        ERROR("Unable to get Applications details: ", error.what());
        return Core::ERROR_GENERAL;
    }
    return ERROR_NONE;
}

uint32_t Executor::Cancel(const std::string& handle)
{
    INFO(" ");
    {
        LockGuard lock(taskMutex);
        if (!isCurrentHandle(handle)
            || (currentTask.progress >= stageBase[enumToInt(OperationStage::EXTRACTING)])) {
            return ERROR_WRONG_PARAMS;
        } else {
            currentTask.cancelled.store(true);
        }
    }
    worker.join();
    return ERROR_NONE;
}

bool Executor::isCurrentHandle(const std::string& aHandle)
{
    return ((!currentTask.handle.empty()) && (currentTask.handle == aHandle));
}

uint32_t Executor::SetMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key,
                         const std::string& value)
{
    try {
        dataBase->SetMetadata(type, id, version, key, value);
    } catch (std::exception& error) {
        ERROR("Unable to set metadata: ", error.what());
        return Core::ERROR_GENERAL;
    }
    return ERROR_NONE;
}

uint32_t Executor::ClearMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key)
{
    try {
        dataBase->ClearMetadata(type, id, version, key);
    } catch (std::exception& error) {
        ERROR("Unable to clear metadata: ", error.what());
        return Core::ERROR_GENERAL;
    }
    return ERROR_NONE;
}

uint32_t Executor::GetMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         DataStorage::AppMetadata& metadata) const
{
    try {
        metadata = dataBase->GetMetadata(type, id, version);
    } catch (std::exception& error) {
        ERROR("Unable to get metadata: ", error.what());
        return Core::ERROR_GENERAL;
    }
    return ERROR_NONE;
}

void Executor::handleDirectories()
{
    Filesystem::createDirectory(config.getAppsPath() + Filesystem::LISA_EPOCH);
    Filesystem::createDirectory(config.getAppsStoragePath() + Filesystem::LISA_EPOCH);
    Filesystem::removeAllDirectoriesExcept(config.getAppsPath(), Filesystem::LISA_EPOCH);
    Filesystem::removeAllDirectoriesExcept(config.getAppsStoragePath(), Filesystem::LISA_EPOCH);
}

void Executor::initializeDataBase(const std::string& dbPath)
{
    std::string path = dbPath + Filesystem::LISA_EPOCH + '/';
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

    worker = std::thread{[=]
    {
        taskRunner(std::move(task));
    }};
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
    catch(CancelledException& exc){
        // nothing to do, cancelled flag already set
    }
    catch(std::exception& exc){
        ERROR("exception running ", task, ": ", exc.what());
        status = OperationStatus::FAILED;
        details = exc.what();
    }

    auto cancelled{false};
    {
        LockGuard lock(taskMutex);
        cancelled = currentTask.cancelled.load();
        if (! cancelled){
            worker.detach();
            handle = currentTask.handle;
        }
        currentTask.reset();
    }

    INFO("scheduled ", currentTask, (cancelled ? " cancelled" : " done"));

    if (! cancelled)
    {
        operationStatusCallback(handle, status, details);
    }
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

    auto authMethod = getAuthenticationMethod(type.c_str(), id.c_str(), url.c_str());
    if(NONE != authMethod) {
        std::string message = std::string{} + "Authentication method unsupported: " + std::to_string(authMethod);
        throw std::runtime_error(message);
    }

    auto appSubPath = Filesystem::createAppPath(type, id, version);
    INFO("appSubPath: ", appSubPath);

    auto tmpPath = config.getAppsTmpPath();
    auto tmpDirPath = tmpPath + appSubPath;
    Filesystem::ScopedDir scopedTmpDir{tmpDirPath};

    Downloader downloader{url, *this};

    auto downloadSize = downloader.getContentLength();
    if (downloadSize == 0) {
        std::string message = std::string{} + "app download size unknown or could not be determined";
        throw std::runtime_error(message);
    }
    auto tmpFreeSpace = Filesystem::getFreeSpace(tmpDirPath);

    INFO("download size: ", downloadSize / 1024, " Kb, free tmp space: ", tmpFreeSpace / 1024, " Kb");

    if (downloadSize > tmpFreeSpace) {
        std::string message = std::string{} + "not enough space on " + tmpPath + " (available: "
                + std::to_string(tmpFreeSpace / 1024) +" Kb, required: " + std::to_string(downloadSize / 1024) + " Kb)";
        throw std::runtime_error(message);
    }

    auto tmpFilePath = tmpDirPath + extractFilename(url);

    downloader.get(tmpFilePath);

    const std::string appsPath = config.getAppsPath() + appSubPath;
    INFO("creating ", appsPath);
    Filesystem::ScopedDir scopedAppDir{appsPath};

    setProgress(0, OperationStage::EXTRACTING);
    INFO("unpacking ", tmpFilePath, "to ", appsPath);
    Archive::unpack(tmpFilePath, appsPath);

    auto appStorageSubPath = Filesystem::createAppPath(type, id);
    auto appStoragePath = config.getAppsStoragePath() + appStorageSubPath;

    INFO("creating storage ", appStoragePath);
    Filesystem::ScopedDir scopedAppStorageDir{appStoragePath};

    setProgress(0, OperationStage::UPDATING_DATABASE);
    dataBase->AddInstalledApp(type, id, version, url, appName, category, appSubPath, appStorageSubPath);

    // everything went fine, mark app directories to not be removed
    scopedAppDir.commit();
    scopedAppStorageDir.commit();

    setProgress(0, OperationStage::FINISHED);

    doMaintenance();

    INFO("finished");
}

void Executor::doUninstall(std::string type, std::string id, std::string version, std::string uninstallType)
{
    INFO("type=", type, " id=", id, " version=", version, " uninstallType=", uninstallType);

    dataBase->RemoveInstalledApp(type, id, version);

    // TODO should "id" be also removed? what if two versions are installed?
    auto appSubPath = Filesystem::createAppPath(type, id, version);
    auto appPath = config.getAppsPath() + appSubPath;

    INFO("removing ", appPath);
    Filesystem::removeDirectory(appPath);

    if (uninstallType == "full") {
        dataBase->RemoveAppData(type, id);
        auto appStoragePath = config.getAppsStoragePath() + Filesystem::createAppPath(type, id);
        INFO("removing storage directory ", appStoragePath);
        Filesystem::removeDirectory(appStoragePath);
    }

    doMaintenance();

    INFO("finished");
}

void Executor::doMaintenance()
{
    try {
        // clear tmp
        Filesystem::removeDirectory(config.getAppsTmpPath());
        Filesystem::createDirectory(config.getAppsTmpPath());

        // remove installed apps data not present in installed_apps
        auto appsPathRoot = config.getAppsPath() + Filesystem::LISA_EPOCH + '/';
        auto foundApps = scanDirectories(appsPathRoot, false);
        for (const auto& app : foundApps) {
            INFO(app);
            if (!dataBase->IsAppInstalled(app.type, app.id, app.version)) {
                ERROR(app, " not found in installed apps, removing dir");
                auto path = config.getAppsPath() + Filesystem::createAppPath(app.type, app.id, app.version);
                Filesystem::removeDirectory(path);
            }
        }

        // remove apps data not present in apps
        auto appsStoragePathRoot = config.getAppsStoragePath() + Filesystem::LISA_EPOCH + '/';
        auto foundAppsStorages = scanDirectories(appsStoragePathRoot, true);
        for (const auto& app : foundAppsStorages) {
            INFO(app);
            if (!dataBase->IsAppData(app.type, app.id)) {
                ERROR(app, " not found in apps, removing dir");
                auto path = config.getAppsStoragePath() + Filesystem::createAppPath(app.type, app.id);
                Filesystem::removeDirectory(path);
            }
        }

        auto appsDetailsList = dataBase->GetAppDetailsList();
        for (const auto& details : appsDetailsList) {
            INFO("details: ", details.id, ":", details.version);

            auto appPaths = dataBase->GetAppsPaths(details.type, details.id, details.version);

            INFO("PATHS APPS:");
            for (const auto& path : appPaths) {
                INFO("path: ", path);
                auto appPath = config.getAppsPath() + path;
                INFO("abs path: ", appPath);

                bool noAppFiles = Filesystem::directoryExists(appPath) ? Filesystem::isEmpty(appPath) : true;
                if (noAppFiles) {
                    dataBase->RemoveInstalledApp(details.type, details.id, details.version);
                }
            }

            auto dataPaths = dataBase->GetDataPaths(details.type, details.id);

            INFO("PATHS DATA:");
            for (const auto& path : dataPaths) {
                INFO("path: ", path);
                auto dataPath = config.getAppsStoragePath() + path;
                INFO("abs path: ", dataPath);
                if (!Filesystem::directoryExists(dataPath)) {
                    Filesystem::createDirectory(dataPath);
                }
            }

        }

#if LISA_APPS_GID
        Filesystem::setPermissionsRecursively(config.getAppsPath(), LISA_APPS_GID, false);
#endif
#if LISA_DATA_GID
        Filesystem::setPermissionsRecursively(config.getAppsStoragePath(), LISA_DATA_GID, true);
#endif

    }
    catch(std::exception& exc) {
        ERROR("ERROR: ", exc.what());
    }
}

void Executor::setProgress(int progress)
{
    setProgress(progress, OperationStage::DOWNLOADING);
}

bool Executor::isCancelled()
{
    return currentTask.cancelled.load();
}

void Executor::setProgress(int stagePercent, OperationStage stage)
{
    int stageIndex = enumToInt(stage);
    int resultPercent = stageBase[stageIndex] + (static_cast<int>(stagePercent * stageFactor[stageIndex]));
    static int prevResultPercent = -1;

    if (resultPercent == prevResultPercent)
      return;
    prevResultPercent = resultPercent;

    INFO("overall: ", resultPercent, "% from stage: ", stage, " progress: ", stagePercent, "%");

    LockGuard lock{taskMutex};
    currentTask.progress = resultPercent;

    std::stringstream ss;
    ss << stage << " " << resultPercent << " %";
    operationStatusCallback(currentTask.handle, OperationStatus::PROGRESS, ss.str());
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
    return out << (status == Executor::OperationStatus::SUCCESS ? "SUCCESS" : status == Executor::OperationStatus::PROGRESS ? "PROGRESS" : "FAILED") ;
}

} // namespace LISA
} // namespace WPEFramework
} // namespace Plugin
