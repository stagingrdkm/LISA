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

#include <string>
#include <stdexcept>

namespace WPEFramework {
namespace Plugin {
namespace LISA {
namespace Archive {

class ArchiveError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

void unpack(const std::string& filePath, const std::string& destinationDir);

} // namespace Archive
} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework


