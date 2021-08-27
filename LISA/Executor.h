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

#include <functional>
#include <string>
#include <thread>

#include <mutex>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

class Executor
{
public:

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

private:

    bool isWorkerBusy() const;
    void executeTask(std::function<void()> task);
    void taskRunner(std::function<void()> task);

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

    using LockGuard = std::lock_guard<std::mutex>;

    std::string currentHandle{};
    std::mutex workerMutex{};
    std::thread worker{};

};

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework
