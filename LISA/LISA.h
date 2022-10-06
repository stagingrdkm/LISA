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
#include "Module.h"
#include <interfaces/json/JsonData_LISA.h>
#include <interfaces/ILISA.h>

namespace WPEFramework {
namespace Plugin {

class LISA : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    public:
        LISA(const LISA&) = delete;
        LISA& operator=(const LISA&) = delete;

        class Notification : public RPC::IRemoteConnection::INotification,
                                public Exchange::ILISA::INotification {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
                explicit Notification(LISA* parent)
                    : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }
                virtual ~Notification() override
                {
                }

            public:
                void operationStatus(const string& handle, const string& operation, const string& type, const string& id,
                                     const string& version, const string& status, const string& details) override
                {
                    _parent.OperationStatus(handle, operation, type, id, version, status, details);
                }

                void Activated(RPC::IRemoteConnection* /* connection */) override
                {
                }
                void Deactivated(RPC::IRemoteConnection* connection) override
                {
                    _parent.Deactivated(connection);
                }

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::ILISA::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

            private:
                LISA& _parent;
            };


        LISA() 
            : _connectionId(0),
            _service(nullptr),
            _lisa(nullptr),
            _notification(this)
        {
        }
        
        virtual ~LISA() 
        {
        }

        BEGIN_INTERFACE_MAP(LISA)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_AGGREGATE(Exchange::ILISA, _lisa)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
        END_INTERFACE_MAP

    private:
        void Deactivated(RPC::IRemoteConnection* connection);
        void OperationStatus(const string& handle, const string& operation, const string& type, const string& id,
                             const string& version, const string& status, const string& details);

    public:
        //   IPlugin methods
        // -------------------------------------------------------------------------------------------------------
        virtual const string Initialize(PluginHost::IShell* service) override;
        virtual void Deinitialize(PluginHost::IShell* service) override;
        virtual string Information() const override;
        
    public:
        static const short API_VERSION_NUMBER_MAJOR;
        static const short API_VERSION_NUMBER_MINOR;

    private:
        uint32_t _connectionId;
        PluginHost::IShell* _service;
        Exchange::ILISA* _lisa;
        Core::Sink<Notification> _notification;

    // JSON-RPC
    private:
        void Register(PluginHost::JSONRPC& module, Exchange::ILISA* destination);
        void Unregister(PluginHost::JSONRPC& module);
        void SendEventOperationStatus(PluginHost::JSONRPC& module, const string& handle, const string& operation,
                                      const string& type, const string& id,
                                      const string& version, const string& status, const string& details);
    };
}
}
