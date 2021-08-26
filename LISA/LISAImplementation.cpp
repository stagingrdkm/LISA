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
        return executor.Install(type, id, version, url, appName, category, handle);
    }

    uint32_t Uninstall(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& uninstallType,
            std::string& handle /* @out */) override
    {
        return executor.Uninstall(type, id, version, uninstallType, handle);
    }

    uint32_t Download(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& resKey,
            const std::string& url,
            std::string& handle /* @out */) override
    {
        handle = "Download";
        return Core::ERROR_NONE;
    }

    uint32_t Reset(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& resetType) override
    {
        return Core::ERROR_NONE;
    }

    class StorageImpl : public ILISA::IStorage
    {
    public:
        enum StorageType
        {
            APPS,
            PERSISTENT
        };

        StorageImpl() = delete;
        StorageImpl(const StorageImpl&) = delete;
        StorageImpl& operator=(const StorageImpl&) = delete;

        StorageImpl(
                const StorageImpl::StorageType storageType,
                const std::string& type,
                const std::string& id,
                const std::string& version) : 
                _storageType(storageType), _type(type), _id(id), _version(version)
        {
        }

        ~StorageImpl() override 
        {
        }

        uint32_t Path(string& path) const override {
            switch (_storageType) {
                case StorageType::APPS:
                    path = "/sample/path/apps";
                    break;
                case StorageType::PERSISTENT:
                    path = "/sample/path/persistent";
                    break;
            }
            return Core::ERROR_NONE;
        }
        
        uint32_t QuotaKB(string& quota) const override {
            quota = "sample quota";
            return Core::ERROR_NONE;
        }
            
        uint32_t UsedKB(string& usedKB) const override {
            usedKB = "sample usedKB";
            return Core::ERROR_NONE;
        }
    private:
        StorageImpl::StorageType _storageType;
        std::string _type;
        std::string _id;
        std::string _version;

    public:
        BEGIN_INTERFACE_MAP(StorageImpl)
            INTERFACE_ENTRY(Exchange::ILISA::IStorage)
        END_INTERFACE_MAP
    }; // StorageImpl

    class StoragePayloadImpl : public ILISA::IStoragePayload
    {
    public:
        StoragePayloadImpl() = delete;
        StoragePayloadImpl(const StoragePayloadImpl&) = delete;
        StoragePayloadImpl& operator=(const StoragePayloadImpl&) = delete;

        StoragePayloadImpl(
                const std::string& type,
                const std::string& id,
                const std::string& version) : 
                _type(type), _id(id), _version(version)
        {
        }

        ~StoragePayloadImpl() override 
        {
        }

        uint32_t Apps(ILISA::IStorage*& storage) const override
        {
            storage = (Core::Service<StorageImpl>::Create<ILISA::IStorage>(StorageImpl::StorageType::APPS, _type, _id, _version));
            return Core::ERROR_NONE;
        }

        uint32_t Persistent(ILISA::IStorage*& storage) const override
        {
            storage = (Core::Service<StorageImpl>::Create<ILISA::IStorage>(StorageImpl::StorageType::PERSISTENT, _type, _id, _version));
            return Core::ERROR_NONE;
        }
    private:
        std::string _type;
        std::string _id;
        std::string _version;

    public:
        BEGIN_INTERFACE_MAP(StoragePayloadImpl)
            INTERFACE_ENTRY(Exchange::ILISA::IStoragePayload)
        END_INTERFACE_MAP
    }; // StoragePayload

    uint32_t GetStorageDetails(const std::string& type,
            const std::string& id,
            const std::string& version,
            ILISA::IStoragePayload*& result) override
    {
        result = (Core::Service<StoragePayloadImpl>::Create<ILISA::IStoragePayload>(type, id, version));
        return Core::ERROR_NONE;
    }
 
    uint32_t SetAuxMetadata(const std::string& type,
            const std::string& id,
            const std::string& version,
            const std::string& auxMetadata) override
    {
        return Core::ERROR_NONE;
    }

    uint32_t GetMetadata(const std::string& type,
            const std::string& id,
            const std::string& version,
            std::string& auxMetadata) override
    {
        auxMetadata = "metadata";
        return Core::ERROR_NONE;
    }

    uint32_t Cancel(const std::string& handle) override
    {
        return Core::ERROR_NONE;
    }

    uint32_t GetProgress(const std::string& handle, uint32_t& progress) override
    {
        progress = 75;
        return Core::ERROR_NONE;
    }

    virtual uint32_t Register(ILISA::INotification* notification) override
    {
        return Core::ERROR_NONE;
    }

    virtual uint32_t Unregister(ILISA::INotification* notification) override
    {
        return Core::ERROR_NONE;
    }

public:
    BEGIN_INTERFACE_MAP(LISAImplementation)
        INTERFACE_ENTRY(Exchange::ILISA)
    END_INTERFACE_MAP

public:
    class AppVersionImpl : public ILISA::IAppVersion 
    {
    public:
        class IteratorImpl : public ILISA::IAppVersion::IIterator {
        public:
            IteratorImpl() = delete;
            IteratorImpl(const IteratorImpl&) = delete;
            IteratorImpl& operator=(const IteratorImpl&) = delete;

            IteratorImpl(const std::list<AppVersionImpl*>& container)
            {
                std::list<AppVersionImpl*>::const_iterator index = container.begin();
                while (index != container.end()) {
                    ILISA::IAppVersion* element = (*index);
                    element->AddRef();
                    _list.push_back(element);
                    index++;
                }
            }
            
            ~IteratorImpl() override
            {
                while (_list.size() != 0) {
                    _list.front()->Release();
                    _list.pop_front();
                }
            }

        public:
            uint32_t Reset() override
            {
                _index = 0;
                return Core::ERROR_NONE;
            }

            bool IsValid() const
            {
                return ((_index != 0) && (_index <= _list.size()));
            }

            uint32_t IsValid(bool& isValid) const override
            {
                isValid = IsValid();
                return Core::ERROR_NONE;
            }

            uint32_t Next(bool& hasNext) override
            {
                if (_index == 0) {
                    _index = 1;
                    _iterator = _list.begin();
                } else if (_index <= _list.size()) {
                    _index++;
                    _iterator++;
                }
                hasNext = IsValid();
                return Core::ERROR_NONE;
            }

            uint32_t Current(ILISA::IAppVersion*& version) const override
            {
                ASSERT(IsValid() == true);
                ILISA::IAppVersion* result = nullptr;
                result = (*_iterator);
                ASSERT(result != nullptr);
                result->AddRef();
                version = result;
                return Core::ERROR_NONE;
            }
        
        public:
            BEGIN_INTERFACE_MAP(AppVersionImpl::IteratorImpl)
                INTERFACE_ENTRY(Exchange::ILISA::IAppVersion::IIterator)
            END_INTERFACE_MAP

        private:
            uint32_t _index = 0;
            std::list<ILISA::IAppVersion*> _list;
            std::list<ILISA::IAppVersion*>::iterator _iterator;
        }; // class IteratorImpl

    public:
        AppVersionImpl() = delete;
        AppVersionImpl(const AppVersionImpl&) = delete;
        AppVersionImpl& operator=(const AppVersionImpl&) = delete;

        AppVersionImpl(const std::string version, const std::string appName, const std::string category, const std::string url) 
            : _version(version), _appName(appName), _category(category), _url(url)
        {
        }

        ~AppVersionImpl() override 
        {
        }

        uint32_t Version(std::string& version) const override
        {
            version = _version;
            return Core::ERROR_NONE;
        }

        uint32_t AppName(std::string& appName) const override 
        {
            appName = _appName;
            return Core::ERROR_NONE;
        }

        uint32_t Category(std::string& category) const override
        {
            category = _category;
            return Core::ERROR_NONE;
        }

        uint32_t Url(std::string& url) const override
        {
            url = _url;
            return Core::ERROR_NONE;
        }

    private:
        std::string _version;
        std::string _appName;
        std::string _category;
        std::string _url;

    public:
        BEGIN_INTERFACE_MAP(AppVersionImpl)
            INTERFACE_ENTRY(Exchange::ILISA::IAppVersion)
        END_INTERFACE_MAP    
    }; // class AppVersionImpl

public:
    class AppImpl : public ILISA::IApp 
    {
    public:
        class IteratorImpl : public ILISA::IApp::IIterator {
        public:
            IteratorImpl() = delete;
            IteratorImpl(const IteratorImpl&) = delete;
            IteratorImpl& operator=(const IteratorImpl&) = delete;

            IteratorImpl(const std::list<AppImpl*>& container)
            {
                std::list<AppImpl*>::const_iterator index = container.begin();
                while (index != container.end()) {
                    ILISA::IApp* element = (*index);
                    element->AddRef();
                    _list.push_back(element);
                    index++;
                }
            }
            
            ~IteratorImpl() override
            {
                while (_list.size() != 0) {
                    _list.front()->Release();
                    _list.pop_front();
                }
            }

        public:
            uint32_t Reset() override
            {
                _index = 0;
                return Core::ERROR_NONE;
            }

            bool IsValid() const
            {
                return ((_index != 0) && (_index <= _list.size()));
            }

            uint32_t IsValid(bool& isValid) const override
            {
                isValid = IsValid();
                return Core::ERROR_NONE;
            }
            
            uint32_t Next(bool& hasNext) override
            {
                if (_index == 0) {
                    _index = 1;
                    _iterator = _list.begin();
                } else if (_index <= _list.size()) {
                    _index++;
                    _iterator++;
                }
                hasNext = IsValid();
                return Core::ERROR_NONE;
            }
  
            uint32_t Current(ILISA::IApp*& app) const override
            {
                ASSERT(IsValid() == true);
                ILISA::IApp* result = nullptr;
                result = (*_iterator);
                ASSERT(result != nullptr);
                result->AddRef();
                app = result;                
                return Core::ERROR_NONE;
            }
            
        public:
            BEGIN_INTERFACE_MAP(AppImpl::IteratorImpl)
                INTERFACE_ENTRY(Exchange::ILISA::IApp::IIterator)
            END_INTERFACE_MAP

        private:
            uint32_t _index = 0;
            std::list<ILISA::IApp*> _list;
            std::list<ILISA::IApp*>::iterator _iterator;
        }; // class IteratorImpl

    public:
        AppImpl() = delete;
        AppImpl(const AppImpl&) = delete;
        AppImpl& operator=(const AppImpl&) = delete;

        AppImpl(const std::string type, const std::string id, const std::list<AppVersionImpl*>& versions) 
            : _type(type), _id(id)
        {
            std::list<AppVersionImpl*>::const_iterator index = versions.begin();
            while (index != versions.end()) {
                AppVersionImpl* element = (*index);
                element->AddRef();
                _versions.push_back(element);
                index++;
            }
        }

        ~AppImpl() override 
        {
            while (_versions.size() != 0) {
                _versions.front()->Release();
                _versions.pop_front();
            }
        }
           
        uint32_t Type(string& type) const override 
        { 
            type = _type;
            return Core::ERROR_NONE;
        }

        uint32_t Id(string& id) const override 
        {
            id = _id;
            return Core::ERROR_NONE;
        }
        
        uint32_t Installed(ILISA::IAppVersion::IIterator*& versions) const override
        {
            versions = Core::Service<AppVersionImpl::IteratorImpl>::Create<ILISA::IAppVersion::IIterator>(_versions);
            return Core::ERROR_NONE;
        }

    private:
        std::string _type;
        std::string _id;
        std::list<AppVersionImpl*> _versions;

    public:
        BEGIN_INTERFACE_MAP(AppImpl)
            INTERFACE_ENTRY(Exchange::ILISA::IApp)
        END_INTERFACE_MAP
    }; // class AppImpl

public:
    class AppsPayloadImpl : public ILISA::IAppsPayload
    {
    public:
        AppsPayloadImpl() = delete;
        AppsPayloadImpl(const AppsPayloadImpl&) = delete;
        AppsPayloadImpl& operator=(const AppsPayloadImpl&) = delete;

        AppsPayloadImpl(const std::list<AppImpl*>&  apps)
        {
            std::list<AppImpl*>::const_iterator index = apps.begin();
            while (index != apps.end()) {
                AppImpl* element = (*index);
                element->AddRef();
                _apps.push_back(element);
                index++;
            }    
        }

        ~AppsPayloadImpl() override 
        {
            while (_apps.size() != 0) {
                _apps.front()->Release();
                _apps.pop_front();
            }
        }

        uint32_t Apps(ILISA::IApp::IIterator*& apps) const override
        {
            apps = (Core::Service<AppImpl::IteratorImpl>::Create<ILISA::IApp::IIterator>(_apps));
            return Core::ERROR_NONE;
        }
    private:
        std::list<AppImpl*> _apps;

    public:
        BEGIN_INTERFACE_MAP(AppsPayloadImpl)
            INTERFACE_ENTRY(Exchange::ILISA::IAppsPayload)
        END_INTERFACE_MAP
    }; // AppsPayloadImpl
    
    /* @brief List installed applications. */
    uint32_t GetList(
        const std::string& type,
        const std::string& id,
        const std::string& version,
        const std::string& appName,
        const std::string& category,
        IAppsPayload*& result) const override
    {
        TRACE(Trace::Information, (_T("LISA GetList")));
    
        // Create versions of the apps
        std::list<AppVersionImpl*> app1_versions;
        AppVersionImpl* app1_version1 = Core::Service<AppVersionImpl>::Create<AppVersionImpl>(
            Core::ToString("1.0.0"), Core::ToString("app1"), Core::ToString("category1"), Core::ToString("http://app1.domain.com/v1"));
        AppVersionImpl* app1_version2 = Core::Service<AppVersionImpl>::Create<AppVersionImpl>(
            Core::ToString("1.1.0"), Core::ToString("app1"), Core::ToString("category1"), Core::ToString("http://app1.domain.com/v11"));

        app1_versions.push_back(app1_version1);
        app1_versions.push_back(app1_version2);

        std::list<AppVersionImpl*> app2_versions;
        AppVersionImpl* app2_version1 = Core::Service<AppVersionImpl>::Create<AppVersionImpl>(
            Core::ToString("2.0.0"), Core::ToString("app2"), Core::ToString("category2"), Core::ToString("http://app2.domain.com/v2"));
        app2_versions.push_back(app2_version1);

        // Create apps
        AppImpl* app1 = Core::Service<AppImpl>::Create<AppImpl>(
            Core::ToString("app1_type"), Core::ToString("id.app1"), app1_versions);
        AppImpl* app2 = Core::Service<AppImpl>::Create<AppImpl>(
            Core::ToString("app2_type"), Core::ToString("id.app2"), app2_versions);
        std::list<AppImpl*> apps;
        apps.push_back(app1);
        apps.push_back(app2);

        TRACE(Trace::Information, (_T("LISA GetList apps:%d"), apps.size()));
        
        // Create apps payload which will be returned as the result
        ILISA::IAppsPayload* appsPayload = Core::Service<AppsPayloadImpl>::Create<ILISA::IAppsPayload>(apps);
        result = appsPayload;
        return Core::ERROR_NONE;
    }
private:
    LISA::Executor executor{};
};

SERVICE_REGISTRATION(LISAImplementation, 1, 0);
}
}