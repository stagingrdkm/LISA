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

#include "File.h"

#include <cassert>

namespace WPEFramework {
namespace Plugin {
namespace LISA {
namespace Filesystem {

File::File(const std::string& path)
{
    if (! path.empty()) {
        file = fopen(path.c_str(), "w");
    }
}

File::~File()
{
    if (file) {
        fclose(file);
    }
}

void* File::getHandle() const
{
    return reinterpret_cast<void*>(file);
}

} // namespace Filesystem
} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework





