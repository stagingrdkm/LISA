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

#include "Module.h"

#include <interfaces/ILISA.h>

namespace WPEFramework {
namespace Plugin {

class LISAImplementation : public Exchange::ILISA {
public:
    LISAImplementation() = default;

    LISAImplementation(const LISAImplementation&) = delete;
    LISAImplementation& operator= (const LISAImplementation&) = delete;

    virtual ~LISAImplementation()
    {
    }

    uint32_t Install(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& url,
            const std::string& appName,
            const std::string& category,
            std::string& handle /* @out */) override
    {
        handle = "Install";
        return (Core::ERROR_NONE);
    }

    uint32_t Uninstall(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& uninstallType,
            std::string& handle /* @out */) override
    {
        handle = "Uninstall";
        return (Core::ERROR_NONE);
    }

    uint32_t Download(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& resKey,
            const std::string& url,
            std::string& handle /* @out */) override
    {
        handle = "Download";
        return (Core::ERROR_NONE);
    }

    uint32_t Reset(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& resetType) override
    {
        return (Core::ERROR_NONE);
    }

    uint32_t GetStorageDetails(const std::string& type,
            const std::string& id,
            const std::string& version,
            std::string& handle /* @out */) override
    {
        handle = "GetStorageDetails";
        return (Core::ERROR_NONE);
    }

    uint32_t GetList(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& appName,
            const std::string& category,
            string& result /* @out */) override
    {
        result = "GetList";
        return (Core::ERROR_NONE);
    }

    uint32_t SetAuxMetadata(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& auxMetadata) override
    {
        return (Core::ERROR_NONE);
    }

    uint32_t GetMetadata(const std::string& type,
            const std::string& id,
            const std::string& version,
            std::string& auxMetadata) override
    {
        auxMetadata = "metadata";
        return (Core::ERROR_NONE);
    }

    uint32_t Cancel(const std::string& handle) override
    {
        return (Core::ERROR_NONE);
    }

    uint32_t GetProgress(const std::string& handle, uint32_t& progress) override
    {
        progress = 75;
        return (Core::ERROR_NONE);
    }

    virtual uint32_t Register(ILISA::INotification* notification) override
    {
        return (Core::ERROR_NONE);
    }

    virtual uint32_t Unregister(ILISA::INotification* notification) override
    {
        return (Core::ERROR_NONE);
    }

    BEGIN_INTERFACE_MAP(LISAImplementation)
        INTERFACE_ENTRY(Exchange::ILISA)
    END_INTERFACE_MAP

public:
};

SERVICE_REGISTRATION(LISAImplementation, 1, 0);
}
}
