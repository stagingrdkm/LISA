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

const short WPEFramework::Plugin::LISA::API_VERSION_NUMBER_MAJOR = 1;
const short WPEFramework::Plugin::LISA::API_VERSION_NUMBER_MINOR = 0;

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(LISA, LISA::API_VERSION_NUMBER_MAJOR, LISA::API_VERSION_NUMBER_MINOR);

    const string LISA::Initialize(PluginHost::IShell* service) {

        TRACE(Trace::Information, (_T("LISA::Initialize")));

        if (_lisa != nullptr) {
            Unregister(*this);
            _lisa->Release();
            _lisa = nullptr;
        }

        string message;

        _lisa = service->Root<Exchange::ILISA>(_connectionId, 2000, _T("LISAImplementation"));
        if (_lisa != nullptr) {
            Exchange::JLISA::Register(*this, _lisa);
            Config config;
            config.FromString(service->ConfigLine());
            std::string path = (config.DbPath.IsSet() && !config.DbPath.Value().empty()) ? config.DbPath.Value() : (service->PersistentPath() + "apps.db");
            _lisa->Configure({path});
            TRACE(Trace::Information, (_T("LISA::Initialize register JSON-RPC API")));
            Register(*this, _lisa);
        }

        if (_lisa == nullptr) {
            TRACE(Trace::Error, (_T("LISA::Initialize - LISA could not be instantiated.")));
            message = _T("LISA could not be instantiated.");
        }

        return message;
    }

    void LISA::Deinitialize(PluginHost::IShell* service)
    {
        TRACE(Trace::Information, (_T("LISA::Deinitialize")));
        ASSERT(_lisa != nullptr);

        if (_lisa != nullptr) {
            Unregister(*this);
            _lisa->Release();
            _lisa = nullptr;
        }
        _connectionId = 0;
    }

    string LISA::Information() const
    {
        return (string());
    }


}
}