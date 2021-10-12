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

#include "Auth.h"


AuthMethod getAuthenticationMethod(const char* appType, const char* id, const char* url)
{
    return NONE;
}

ReturnCode getAPIKey(const char* appType, const char* id, const char* url, char* key, const size_t bufferSize)
{
    return ERROR_OTHER;
}

ReturnCode getCredentials(const char*  appType, const char* id, const char* url, char* user, char* password, const size_t bufferSize)
{
    return ERROR_OTHER;
}

ReturnCode getClientCertsFile(const char* appType, const char* id, const char* url,
                            char* privKeyFileName, char* privKeyPassphrase, char* clientCertFileName, size_t bufferSize)
{
    return ERROR_OTHER;
}

ReturnCode getToken(const char* appType, const char* id, const char* url, char* token, size_t bufferSize)
{
    return ERROR_OTHER;
}
