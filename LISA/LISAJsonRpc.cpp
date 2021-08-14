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

#include "LISA.h"

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
                TRACE(Trace::Information, (_T("LISAJsonRpc Install")));

                std::string result;
                errorCode = destination->Install(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(),
                    params.Url.Value(), 
                    params.AppName.Value(), 
                    params.Category.Value(), result);
                response = result;

                TRACE(Trace::Information, (_T("LISAJsonRpc Install finished with code:%d"), errorCode));
                return errorCode;
            });

        module.Register<UninstallParamsData,Core::JSON::String>(_T("uninstall"), 
            [destination, this](const UninstallParamsData& params, Core::JSON::String& response) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                TRACE(Trace::Information, (_T("LISAJsonRpc Uninstall")));

                std::string result;
                errorCode = destination->Uninstall(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(), 
                    params.UninstallType.Value(), result);
                response = result;

                TRACE(Trace::Information, (_T("LISAJsonRpc Uninstall finished with code:%d"), errorCode));
                return errorCode;
            });

        module.Register<DownloadParamsData,Core::JSON::String>(_T("download"), 
            [destination, this](const DownloadParamsData& params, Core::JSON::String& response) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                TRACE(Trace::Information, (_T("LISAJsonRpc Download")));

                std::string result;
                errorCode = destination->Download(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(), 
                    params.ResKey.Value(),
                    params.ResUrl.Value(), result);
                response = result;

                TRACE(Trace::Information, (_T("LISAJsonRpc Download finished with code:%d"), errorCode));
                return errorCode;
            });

        module.Register<ResetParamsData,void>(_T("reset"), 
            [destination, this](const ResetParamsData& params) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                TRACE(Trace::Information, (_T("LISAJsonRpc Reset")));

                errorCode = destination->Reset(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(), 
                    params.ResetType.Value());

                TRACE(Trace::Information, (_T("LISAJsonRpc Reset finished with code:%d"), errorCode));
                return errorCode;
            });

        module.Register<GetStorageDetailsParamsInfo,StoragepayloadData>(_T("getStorageDetails"), 
            [destination, this](const GetStorageDetailsParamsInfo& params, StoragepayloadData& response) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                Exchange::ILISA::IStoragePayload* result = nullptr;
                Exchange::ILISA::IStorage* iStorage = nullptr;
                std::string val;

                TRACE(Trace::Information, (_T("LISAJsonRpc GetStorageDetails")));

                errorCode = destination->GetStorageDetails(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(), result);

                if (errorCode != Core::ERROR_NONE) {
                    TRACE(Trace::Error, (_T("LISAJsonRpc GetStorageDetails() result:%d"), errorCode));
                    return errorCode;
                }
                
                errorCode = result->Apps(iStorage);
                if (errorCode != Core::ERROR_NONE) {
                    TRACE(Trace::Error, (_T("LISAJsonRpc Apps() result:%d"), errorCode));
                    return errorCode;
                }

                iStorage->Path(val);
                response.Apps.Path = Core::ToString(val);
                iStorage->QuotaKB(val);
                response.Apps.QuotaKB = Core::ToString(val);
                iStorage->UsedKB(val);
                response.Apps.UsedKB = Core::ToString(val);
                  
                errorCode = result->Persistent(iStorage);
                if (errorCode != Core::ERROR_NONE) {
                    TRACE(Trace::Error, (_T("LISAJsonRpc Persistent() result:%d"), errorCode));
                    return errorCode;
                }

                iStorage->Path(val);
                response.Persistent.Path = Core::ToString(val);
                iStorage->QuotaKB(val);
                response.Persistent.QuotaKB = Core::ToString(val);
                iStorage->UsedKB(val);
                response.Persistent.UsedKB = Core::ToString(val);
                    
                TRACE(Trace::Information, (_T("LISAJsonRpc GetStorageDetails finished with code:%d"), errorCode));
                return errorCode;
            });

        module.Register<SetAuxMetadataParamsData,void>(_T("setAuxMetadata"), 
            [destination, this](const SetAuxMetadataParamsData& params) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                TRACE(Trace::Information, (_T("LISAJsonRpc SetAuxMetadata")));

                errorCode = destination->SetAuxMetadata(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(), 
                    params.AuxMetadata.Value());

                TRACE(Trace::Information, (_T("LISAJsonRpc SetAuxMetadata finished with code:%d"), errorCode));
                return errorCode;
            });

        // For some reason JsonGenerator doesn't generate separate ParamsInfo/ParamsData
        // classes for functions that have the same input params. In this case GetMetadata
        // and GetStorageDetails have the same 3 input params and only GetStorageDetailsParamsInfo
        // was generated. Despite there is no GetMetadataParamsInfo we can use the params for
        // GetStorageDetails in it's place.
        module.Register<GetStorageDetailsParamsInfo,Core::JSON::String>(_T("getMetadata"), 
            [destination, this](const GetStorageDetailsParamsInfo& params, Core::JSON::String& response) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                TRACE(Trace::Information, (_T("LISAJsonRpc GetMetadata")));

                std::string result;
                errorCode = destination->GetMetadata(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(), result);
                response = result;

                TRACE(Trace::Information, (_T("LISAJsonRpc GetMetadata finished with code:%d"), errorCode));
                return errorCode;
            });

        module.Register<CancelParamsInfo,void>(_T("cancel"), 
            [destination, this](const CancelParamsInfo& params) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                TRACE(Trace::Information, (_T("LISAJsonRpc Cancel")));

                errorCode = destination->Cancel(
                    params.Handle.Value());

                TRACE(Trace::Information, (_T("LISAJsonRpc Cancel finished with code:%d"), errorCode));
                return errorCode;
            });

        // CancelParamsInfo is used instead of GetProgressParamsInfo which 
        // simply wasn't generated. See the comment for getMetadata as this 
        // is the same case
        module.Register<CancelParamsInfo,Core::JSON::DecUInt64>(_T("getProgress"), 
            [destination, this](const CancelParamsInfo& params, Core::JSON::DecUInt64& response) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                TRACE(Trace::Information, (_T("LISAJsonRpc GetProgress")));

                uint32_t result;
                errorCode = destination->GetProgress(
                    params.Handle.Value(), result);
                response = result;

                TRACE(Trace::Information, (_T("LISAJsonRpc GetProgress finished with code:%d"), errorCode));
                return errorCode;
            });

        module.Register<GetListParamsData,AppslistpayloadData>(_T("getList"), 
            [destination, this](const GetListParamsData& params, AppslistpayloadData& response) -> uint32_t 
            {
                uint32_t errorCode = Core::ERROR_NONE;
                Exchange::ILISA::IAppsPayload* result = nullptr;
                Exchange::ILISA::IApp::IIterator* apps = nullptr;
                Exchange::ILISA::IAppVersion::IIterator* versions = nullptr;
                Exchange::ILISA::IApp* iApp = nullptr;
                Exchange::ILISA::IAppVersion* iVersion = nullptr;
                bool hasNext = false;
                std::string val;

                TRACE(Trace::Information, (_T("LISAJsonRpc GetList")));

                errorCode = destination->GetList(
                    params.Type.Value(),
                    params.Id.Value(), 
                    params.Version.Value(), 
                    params.AppName.Value(), 
                    params.Category.Value(), result);

                if (errorCode != Core::ERROR_NONE) {
                    TRACE(Trace::Error, (_T("LISAJsonRpc GetList() result:%d"), errorCode));
                    return errorCode;
                }
                
                errorCode = result->Apps(apps);
                if (errorCode != Core::ERROR_NONE) {
                    TRACE(Trace::Error, (_T("LISAJsonRpc Apps() result:%d"), errorCode));
                    return errorCode;
                }
                
                // Loop through apps in the response
                while ((errorCode = apps->Next(hasNext)) == Core::ERROR_NONE && hasNext)
                {
                    errorCode = apps->Current(iApp);
                    if (errorCode != Core::ERROR_NONE) {
                        TRACE(Trace::Error, (_T("LISAJsonRpc Current() result:%d"), errorCode));
                        return errorCode;
                    }
                    
                    AppslistpayloadData::AppData app;
                    iApp->Id(val);
                    app.Id = Core::ToString(val);
                    TRACE(Trace::Information, (_T("LISAJsonRpc GetList app:%s"), val.c_str()));
                    iApp->Type(val);
                    app.Type = Core::ToString(val);
                    
                    errorCode = iApp->Installed(versions);
                    if (errorCode != Core::ERROR_NONE) {
                       TRACE(Trace::Error, (_T("LISAJsonRpc Installed() result:%d"), errorCode));
                       return errorCode;
                    }
                    
                    // Loop through versions of a single app
                    while ((errorCode = versions->Next(hasNext)) == Core::ERROR_NONE && hasNext)
                    {
                        errorCode = versions->Current(iVersion);
                        if (errorCode != Core::ERROR_NONE) {
                            TRACE(Trace::Error, (_T("LISAJsonRpc Current() result:%d"), errorCode));
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
                TRACE(Trace::Information, (_T("LISAJsonRpc GetList finished with code:%d"), errorCode));
                return errorCode;
            });
    }

    void LISA::Unregister(PluginHost::JSONRPC& module)
    {
        module.Unregister(_T("getList"));
    }
    

} // namespace Plugin

}
