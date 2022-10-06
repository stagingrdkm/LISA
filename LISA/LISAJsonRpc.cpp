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

#include "Debug.h"
#include "LISA.h"

#include <memory>

namespace { // anonymous

template<typename RPCObject>
struct RPCDeleter
{
    void operator()(RPCObject* ptr)
    {
        if (ptr) {
            ptr->Release();
        }
    }
};

template<typename RPCObject>
using RPCUnique = std::unique_ptr<RPCObject, RPCDeleter<RPCObject> >;

// just for readability
template<typename RPCObject>
RPCUnique<RPCObject> makeUniqueRpc(RPCObject* ptr)
{
    return RPCUnique<RPCObject>(ptr);
}

} // namespace anonymous

namespace WPEFramework {
namespace Plugin {

    using namespace JsonData::LISA;

    void LISA::Register(PluginHost::JSONRPC& module, Exchange::ILISA* destination)
    {
        ASSERT(destination != nullptr);

        module.Register<InstallParamsData,Core::JSON::String>(_T("install"),
            [destination, this](const InstallParamsData& params, Core::JSON::String& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("Install");

                std::string result;
                errorCode = destination->Install(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(),
                    params.Url.Value(),
                    params.AppName.Value(),
                    params.Category.Value(), result);
                response = result;

                INFO("Install finished with code: ", errorCode);
                return errorCode;
            });

        module.Register<UninstallParamsData,Core::JSON::String>(_T("uninstall"),
            [destination, this](const UninstallParamsData& params, Core::JSON::String& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("Uninstall");

                std::string result;
                errorCode = destination->Uninstall(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(),
                    params.UninstallType.Value(), result);
                response = result;

                INFO("Uninstall finished with code: ", errorCode);
                return errorCode;
            });

        // For some reason JsonGenerator doesn't generate separate ParamsInfo/ParamsData
        // classes for functions that have the same input params. Re-use other class.
        using HandleResultData = CancelParamsInfo;
        module.Register<LockParamsData,HandleResultData>(_T("lock"),
            [destination, this](const LockParamsData& params, HandleResultData& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                std::string val;
                Exchange::ILISA::IHandleResult* result;

                INFO("Lock");

                errorCode = destination->Lock(
                        params.Type.Value(),
                        params.Id.Value(),
                        params.Version.Value(),
                        params.Reason.Value(),
                        params.Owner.Value(),
                        result);
                if (errorCode != Core::ERROR_NONE) {
                    ERROR("LISAJsonRpc Lock() result: ", errorCode);
                    return errorCode;
                }

                result->Handle(val);
                response.Handle = Core::ToString(val);

                INFO("Lock finished with code: ", errorCode);
                return errorCode;
            });

        // For some reason JsonGenerator doesn't generate separate ParamsInfo/ParamsData
        // classes for functions that have the same input params. Re-use other class.
        using UnlockParamsData = CancelParamsInfo;
        module.Register<UnlockParamsData,Core::JSON::Container>(_T("unlock"),
            [destination, this](const UnlockParamsData& params, Core::JSON::Container& response) -> uint32_t
            {
               uint32_t errorCode = Core::ERROR_NONE;
               INFO("Unlock");

               errorCode = destination->Unlock(
                       params.Handle.Value());

               INFO("Unlock finished with code: ", errorCode);
               return errorCode;
            });

        // For some reason JsonGenerator doesn't generate separate ParamsInfo/ParamsData
        // classes for functions that have the same input params. Re-use other class.
        using GetLockInfoParamsInfo = GetStorageDetailsParamsInfo;
        module.Register<GetLockInfoParamsInfo,LockinfoData>(_T("getLockInfo"),
             [destination, this](const GetLockInfoParamsInfo& params, LockinfoData& response) -> uint32_t
             {
                 uint32_t errorCode = Core::ERROR_NONE;
                 std::string val;
                 Exchange::ILISA::ILockInfo* result;

                 INFO("GetLockInfo");

                 errorCode = destination->GetLockInfo(
                         params.Type.Value(),
                         params.Id.Value(),
                         params.Version.Value(),
                         result);
                 if (errorCode != Core::ERROR_NONE) {
                     ERROR("LISAJsonRpc GetLockInfo() result: ", errorCode);
                     return errorCode;
                 }

                 result->Reason(val);
                 response.Reason = Core::ToString(val);
                 result->Owner(val);
                 response.Owner = Core::ToString(val);

                 INFO("GetLockInfo finished with code: ", errorCode);
                 return errorCode;
             });

        module.Register<DownloadParamsData,Core::JSON::String>(_T("download"),
            [destination, this](const DownloadParamsData& params, Core::JSON::String& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("Download");

                std::string result;
                errorCode = destination->Download(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(),
                    params.ResKey.Value(),
                    params.ResUrl.Value(), result);
                response = result;

                INFO("Download finished with code: ", errorCode);
                return errorCode;
            });

        module.Register<ResetParamsData,Core::JSON::String>(_T("reset"),
            [destination, this](const ResetParamsData& params, Core::JSON::String& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("Reset");
                std::string result{};
                errorCode = destination->Reset(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(),
                    params.ResetType.Value());
                response = result;

                INFO("Reset finished with code: ", errorCode);
                return errorCode;
            });

        module.Register<GetStorageDetailsParamsInfo,StoragepayloadData>(_T("getStorageDetails"),
            [destination, this](const GetStorageDetailsParamsInfo& params, StoragepayloadData& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;

                INFO(" ");

                Exchange::ILISA::IStoragePayload* storagePayloadRaw = nullptr;
                errorCode = destination->GetStorageDetails(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(), storagePayloadRaw);
                auto storagePayload = makeUniqueRpc(storagePayloadRaw);

                if (errorCode != Core::ERROR_NONE) {
                   ERROR("result: ", errorCode);
                   return errorCode;
                }

                Exchange::ILISA::IStorage* iStorageRaw = nullptr;
                errorCode = storagePayload->Apps(iStorageRaw);
                auto iStorage = makeUniqueRpc(iStorageRaw);
                if (errorCode != Core::ERROR_NONE) {
                    ERROR("Apps() result: ", errorCode);
                    return errorCode;
                }

                std::string val;
                iStorage->Path(val);
                response.Apps.Path = Core::ToString(val);
                iStorage->QuotaKB(val);
                response.Apps.QuotaKB = Core::ToString(val);
                iStorage->UsedKB(val);
                response.Apps.UsedKB = Core::ToString(val);

                errorCode = storagePayload->Persistent(iStorageRaw);
                iStorage.reset(iStorageRaw);
                if (errorCode != Core::ERROR_NONE) {
                    ERROR("Persistent result: ");
                    return errorCode;
                }

                iStorage->Path(val);
                response.Persistent.Path = Core::ToString(val);
                iStorage->QuotaKB(val);
                response.Persistent.QuotaKB = Core::ToString(val);
                iStorage->UsedKB(val);
                response.Persistent.UsedKB = Core::ToString(val);

                INFO("GetStorageDetails finished with code: ", errorCode);
                return errorCode;
            });

        module.Register<SetAuxMetadataParamsData,void>(_T("setAuxMetadata"),
            [destination, this](const SetAuxMetadataParamsData& params) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("SetAuxMetadata");

                errorCode = destination->SetAuxMetadata(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(),
                    params.Key.Value(),
                    params.Value.Value());

                INFO("SetAuxMetadata finished with code: ", errorCode);
                return errorCode;
            });

        module.Register<ClearAuxMetadataParamsData,void>(_T("clearAuxMetadata"),
            [destination, this](const ClearAuxMetadataParamsData& params) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("ClearAuxMetadata");

                errorCode = destination->ClearAuxMetadata(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(),
                    params.Key.Value());

                INFO("ClearAuxMetadata finished with code: ", errorCode);
                return errorCode;
            });


        // For some reason JsonGenerator doesn't generate separate ParamsInfo/ParamsData
        // classes for functions that have the same input params. In this case GetMetadata
        // and GetStorageDetails have the same 3 input params and only GetStorageDetailsParamsInfo
        // was generated. Despite there is no GetMetadataParamsInfo we can use the params for
        // GetStorageDetails in it's place.
        using GetMetadataParamsInfo = GetStorageDetailsParamsInfo;
        module.Register<GetMetadataParamsInfo,MetadatapayloadData>(_T("getMetadata"),
            [destination, this](const GetMetadataParamsInfo& params, MetadatapayloadData& response) -> uint32_t
            {
                INFO("GetMetadata");
                uint32_t errorCode = Core::ERROR_NONE;
                Exchange::ILISA::IMetadataPayload* resultRaw = nullptr;
                Exchange::ILISA::IKeyValueIterator* resourcesRaw = nullptr;
                Exchange::ILISA::IKeyValueIterator* auxMetadataRaw = nullptr;
                Exchange::ILISA::IKeyValue* iKeyValueRaw = nullptr;
                bool hasNext = false;
                std::string val;

                errorCode = destination->GetMetadata(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(), resultRaw);

                if (errorCode != Core::ERROR_NONE) {
                    ERROR("LISAJsonRpc GetMetadata() result: ", errorCode);
                    return errorCode;
                }

                auto result = makeUniqueRpc(resultRaw);

                errorCode = result->Resources(resourcesRaw);
                if (errorCode != Core::ERROR_NONE) {
                    ERROR("LISAJsonRpc Resources() result: ", errorCode);
                    return errorCode;
                }

                auto resources = makeUniqueRpc(resourcesRaw);

                errorCode = result->AuxMetadata(auxMetadataRaw);
                if (errorCode != Core::ERROR_NONE) {
                    ERROR("LISAJsonRpc AuxMetadata() result: ", errorCode);
                    return errorCode;
                }

                auto auxMetadata = makeUniqueRpc(auxMetadataRaw);

                result->AppName(val);
                response.AppName = Core::ToString(val);
                result->Category(val);
                response.Category = Core::ToString(val);
                result->Url(val);
                response.Url = Core::ToString(val);

                // Loop through resources in the response
                while ((errorCode = resources->Next(hasNext)) == Core::ERROR_NONE && hasNext)
                {
                    errorCode = resources->Current(iKeyValueRaw);
                    if (errorCode != Core::ERROR_NONE) {
                        ERROR("LISAJsonRpc getMetadata Current() result: ", errorCode);
                        return errorCode;
                    }
                    auto iKeyValue = makeUniqueRpc(iKeyValueRaw);

                    KeyvalueInfo resource;
                    iKeyValue->Key(val);
                    resource.Key = Core::ToString(val);
                    INFO("LISAJsonRpc GetMetadata resource key: ", val);
                    iKeyValue->Value(val);
                    resource.Value = Core::ToString(val);
                    INFO("LISAJsonRpc GetMetadata resource value: ", val);
                    response.Resources.Add(resource);
                }

                // Loop through aux metadata in the response
                while ((errorCode = auxMetadata->Next(hasNext)) == Core::ERROR_NONE && hasNext)
                {
                    errorCode = auxMetadata->Current(iKeyValueRaw);
                    if (errorCode != Core::ERROR_NONE) {
                        ERROR("LISAJsonRpc getMetadata Current() result: ", errorCode);
                        return errorCode;
                    }
                    auto iKeyValue = makeUniqueRpc(iKeyValueRaw);

                    KeyvalueInfo keyValue;
                    iKeyValue->Key(val);
                    keyValue.Key = Core::ToString(val);
                    INFO("LISAJsonRpc GetMetadata keyValue key: ", val);
                    iKeyValue->Value(val);
                    keyValue.Value = Core::ToString(val);
                    INFO("LISAJsonRpc GetMetadata keyValue value: ", val);
                    response.AuxMetadata.Add(keyValue);
                }

                INFO("GetMetadata finished with code: ", errorCode);
                return errorCode;
            });

        module.Register<CancelParamsInfo,Core::JSON::String>(_T("cancel"),
            [destination, this](const CancelParamsInfo& params, Core::JSON::String& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("Cancel");
                std::string result{};
                errorCode = destination->Cancel(
                    params.Handle.Value());
                response = result;

                INFO("Cancel finished with code: ", errorCode);
                return errorCode;
            });

        // CancelParamsInfo is used instead of GetProgressParamsInfo which
        // simply wasn't generated. See the comment for getMetadata as this
        // is the same case
        using GetProgressParamsInfo = CancelParamsInfo;
        module.Register<GetProgressParamsInfo,Core::JSON::DecUInt64>(_T("getProgress"),
            [destination, this](const GetProgressParamsInfo& params, Core::JSON::DecUInt64& response) -> uint32_t
            {
                uint32_t errorCode = Core::ERROR_NONE;
                INFO("GetProgress");

                uint32_t result;
                errorCode = destination->GetProgress(
                    params.Handle.Value(), result);
                response = result;

                INFO("GetProgress finished with code: ", errorCode);
                return errorCode;
            });

        module.Register<GetListParamsData,AppslistpayloadData>(_T("getList"),
            [destination, this](const GetListParamsData& params, AppslistpayloadData& response) -> uint32_t
            {
               using namespace Exchange;

                INFO("GetList");

                uint32_t errorCode = Core::ERROR_NONE;
                ILISA::IAppsPayload* appListRaw = nullptr;
                errorCode = destination->GetList(
                    params.Type.Value(),
                    params.Id.Value(),
                    params.Version.Value(),
                    params.AppName.Value(),
                    params.Category.Value(), appListRaw);
                auto appList = makeUniqueRpc(appListRaw);

                if (errorCode != Core::ERROR_NONE) {
                    ERROR("GetList() result: ", errorCode);
                    return errorCode;
                }

                ILISA::IApp::IIterator* appsRaw = nullptr;
                errorCode = appList->Apps(appsRaw);
                auto apps = makeUniqueRpc(appsRaw);
                if (errorCode != Core::ERROR_NONE) {
                    ERROR("Apps() result: ", errorCode);
                    return errorCode;
                }

                // Loop through apps in the response
                bool hasNext = false;
                while ((errorCode = apps->Next(hasNext)) == Core::ERROR_NONE && hasNext)
                {
                    ILISA::IApp* iAppRaw = nullptr;
                    errorCode = apps->Current(iAppRaw);
                    auto iApp = makeUniqueRpc(iAppRaw);
                    if (errorCode != Core::ERROR_NONE) {
                        ERROR("Current() result: ", errorCode);
                        return errorCode;
                    }

                    AppslistpayloadData::AppData app;
                    std::string val;
                    iApp->Id(val);
                    app.Id = Core::ToString(val);
                    INFO("GetList app: ", val);
                    iApp->Type(val);
                    app.Type = Core::ToString(val);

                    ILISA::IAppVersion::IIterator* versionsRaw = nullptr;
                    errorCode = iApp->Installed(versionsRaw);
                    auto versions = makeUniqueRpc(versionsRaw);
                    if (errorCode != Core::ERROR_NONE) {
                       ERROR("Installed() result: ", errorCode);
                       return errorCode;
                    }

                    // Loop through versions of a single app
                    while ((errorCode = versions->Next(hasNext)) == Core::ERROR_NONE && hasNext)
                    {
                        ILISA::IAppVersion* iVersionRaw = nullptr;
                        errorCode = versions->Current(iVersionRaw);
                        auto iVersion = makeUniqueRpc(iVersionRaw);
                        if (errorCode != Core::ERROR_NONE) {
                            ERROR("Current() result: ", errorCode);
                            return errorCode;
                        }

                        AppslistpayloadData::AppData::InstalledappData installedApp;
                        iVersion->Version(val);
                        installedApp.Version = Core::ToString(val);
                        iVersion->AppName(val);
                        installedApp.AppName = Core::ToString(val);
                        iVersion->Url(val);
                        installedApp.Url = Core::ToString(val);
                        app.Installed.Add(installedApp);
                    }
                    response.Apps.Add(app);
                }

                INFO("GetList finished with code: ", errorCode);
                return errorCode;
            });
    }

    void LISA::Unregister(PluginHost::JSONRPC& module)
    {
        module.Unregister(_T("install"));
        module.Unregister(_T("uninstall"));
        module.Unregister(_T("download"));
        module.Unregister(_T("getStorageDetails"));
        module.Unregister(_T("setAuxMetadata"));
        module.Unregister(_T("getMetadata"));
        module.Unregister(_T("getProgress"));
        module.Unregister(_T("getList"));
        module.Unregister(_T("lock"));
        module.Unregister(_T("unlock"));
        module.Unregister(_T("getLockInfo"));
    }

    void LISA::SendEventOperationStatus(PluginHost::JSONRPC& module, const string& handle, const string& operation,
                                        const string& type, const string& id,
                                        const string& version, const string& status, const string& details)
    {
        OperationStatusParamsData params;
        params.Handle = handle;
        params.Operation = operation;
        params.Type = type;
        params.Id = id;
        params.Version = version;
        params.Status = status;
        params.Details = details;

        module.Notify(_T("operationStatus"), params);
    }

} // namespace Plugin

}
