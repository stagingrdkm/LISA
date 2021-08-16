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

        LISA() 
            : _connectionId(0),
            _lisa(nullptr)
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
        Exchange::ILISA* _lisa;

    // JSON-RPC
    private:
        void Register(PluginHost::JSONRPC& module, Exchange::ILISA* destination);
        void Unregister(PluginHost::JSONRPC& module);
    };
}
}
