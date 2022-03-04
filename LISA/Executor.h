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

#include "Config.h"
#include "Debug.h"
#include "DataStorage.h"
#include "Downloader.h"

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <thread>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

namespace Filesystem {
struct StorageDetails;
}

class Executor : private DownloaderListener
{
public:
    enum class OperationStatus {
        SUCCESS,
        FAILED,
        PROGRESS
    };

    using OperationStatusCallback = std::function<void (std::string, OperationStatus, std::string)> ;

    Executor(OperationStatusCallback callback) :
        operationStatusCallback(callback)
    {
    }

    uint32_t Configure(const std::string& configString);

    uint32_t Install(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& url,
            const std::string& appName,
            const std::string& category,
            std::string& handle);

    uint32_t Uninstall(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& uninstallType,
            std::string& handle);

    uint32_t GetProgress(const std::string& handle, std::uint32_t& progress);

    uint32_t GetStorageDetails(const std::string& type,
                               const std::string& id,
                               const std::string& version,
                               Filesystem::StorageDetails& details);

    uint32_t GetAppDetailsList(const std::string& type,
                               const std::string& id,
                               const std::string& version,
                               const std::string& appName,
                               const std::string& category,
                               std::vector<DataStorage::AppDetails>& appsDetailsList) const;

    uint32_t Cancel(const std::string& handle);

    uint32_t SetMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key,
                         const std::string& value);

    uint32_t ClearMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         const std::string& key);

    uint32_t GetMetadata(const std::string& type,
                         const std::string& id,
                         const std::string& version,
                         DataStorage::AppMetadata& metadata) const;
private:

    enum class OperationStage
    {
        DOWNLOADING,
        EXTRACTING,
        UPDATING_DATABASE,
        FINISHED,

        COUNT
    };
    static constexpr auto STAGES = enumToInt(OperationStage::COUNT);
    const std::array<int, STAGES> stageBase = {{0, 90, 95, 100}};
    const std::array<double, STAGES> stageFactor = {{90.0 / 100, 5.0 / 100, 5.0 / 100, 0}};

    bool isCurrentHandle(const std::string& aHandle);

    void handleDirectories();
    void initializeDataBase(const std::string& dbpath);

    bool isWorkerBusy() const;
    void executeTask(std::function<void()> task);
    void taskRunner(std::function<void()> task);

    bool isAppInstalled(const std::string& type,
                        const std::string& id,
                        const std::string& version);

    void doInstall(std::string type,
                   std::string id,
                   std::string version,
                   std::string url,
                   std::string appName,
                   std::string category);

    void doUninstall(std::string type,
                     std::string id,
                     std::string version,
                     std::string uninstallType);

    void doMaintenance();

    bool getStorageParamsValid(const std::string& type,
            const std::string& id,
            const std::string& version) const;

    void setProgress(int progress) override;
    bool isCancelled() override;
    void setProgress(int percentValue, OperationStage stage);

    std::unique_ptr<LISA::DataStorage> dataBase;

    using LockGuard = std::lock_guard<std::mutex>;
    struct Task {
        void reset() {
            handle.clear();
            progress = 0;
            cancelled.store(false);
        }
        std::string handle{};
        int progress{0};
        std::atomic_bool cancelled{false};
    };
    Task currentTask{};
    std::thread worker{};
    std::mutex taskMutex{};
    OperationStatusCallback operationStatusCallback;

    Config config{};

    friend std::ostream& operator<<(std::ostream& out, const OperationStatus& status);
    friend std::ostream& operator<<(std::ostream& out, OperationStage stage);
    friend std::ostream& operator<<(std::ostream& out, const Task& task);

};

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
