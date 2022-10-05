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

#include <string>

const short WPEFramework::Plugin::LISA::API_VERSION_NUMBER_MAJOR = 1;
const short WPEFramework::Plugin::LISA::API_VERSION_NUMBER_MINOR = 0;

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(LISA, LISA::API_VERSION_NUMBER_MAJOR, LISA::API_VERSION_NUMBER_MINOR);

    const string LISA::Initialize(PluginHost::IShell* service)
    {
        TRACE_L1("");

        ASSERT(_service == nullptr);
        ASSERT(_lisa == nullptr);

        // Register the Connection::Notification first. The Remote process might die before we get a
        // chance to "register" the sink for these events !!! So do it ahead of instantiation.
        _service = service;
        _service->Register(&_notification);

        string message;
        _lisa = service->Root<Exchange::ILISA>(_connectionId, 2000, _T("LISAImplementation"));
        if (_lisa != nullptr) {
            auto configResult = _lisa->Configure(service->ConfigLine());
            if (configResult == Core::ERROR_NONE) {
                TRACE_L1("LISA::Initialize register notification");
                _lisa->Register(&_notification);

                TRACE_L1("LISA::Initialize register JSON-RPC API");
                Register(*this, _lisa);
            } else {
                TRACE_L1("LISA::Configure failed, reason: %u", configResult);
                message = _T("LISA could not be instantiated - could not initialize database.");
                _lisa->Release();
                _lisa = nullptr;
            }
        }

        if (_lisa == nullptr) {
            TRACE_L1("LISA could not be instantiated.");
            _service->Unregister(&_notification);
            _service = nullptr;
            message = _T("LISA could not be instantiated.");
        }

        return message;
    }

    void LISA::Deinitialize(PluginHost::IShell* service)
    {
        TRACE_L1("");

        ASSERT(_service == service);
        ASSERT(_lisa != nullptr);

        if (_lisa != nullptr) {
            TRACE_L1("unregister notification");
            _service->Unregister(&_notification);
            _lisa->Unregister(&_notification);
            TRACE_L1("unregister JSON-RPC API");
            Unregister(*this);
            _lisa->Release();
        }
        _connectionId = 0;
        _service = nullptr;
        _lisa = nullptr;

        TRACE_L1("done");
    }

    string LISA::Information() const
    {
        return (string());
    }

    void LISA::OperationStatus(const string& handle, const string& operation, const string& type, const string& id,
                               const string& version, const string& status, const string& details)
    {
        INFO("handle: ", handle, " status: ", status, " details: ", details);
        SendEventOperationStatus(*this, handle, operation, type, id, version, status, details);
    }

    void LISA::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {

            ASSERT(_service != nullptr);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }
}
}
