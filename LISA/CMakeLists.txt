# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2021 Liberty Global Service B.V.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(PLUGIN_NAME LISA)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(Boost COMPONENTS system filesystem REQUIRED)
find_package(Sqlite REQUIRED)

add_library(${MODULE_NAME} SHARED
    Archives.cpp
    Config.cpp
    Downloader.cpp
    Executor.cpp
    File.cpp
    Filesystem.cpp
    LISA.cpp
    LISAImplementation.cpp
    SqlDataStorage.cpp
    LISAJsonRpc.cpp
    Module.cpp)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED YES)

if (PLUGIN_LISA_APPS_GID)
  add_definitions(-DLISA_APPS_GID=${PLUGIN_LISA_APPS_GID})
endif (PLUGIN_LISA_APPS_GID)

if (PLUGIN_LISA_DATA_GID)
  add_definitions(-DLISA_DATA_GID=${PLUGIN_LISA_DATA_GID})
endif (PLUGIN_LISA_DATA_GID)

add_subdirectory(AuthModule)

target_link_libraries(${MODULE_NAME}
    PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    PRIVATE ${CURL_LIBRARIES}
    PRIVATE ${LibArchive_LIBRARIES}
    PRIVATE ${Boost_FILESYSTEM_LIBRARY}
    PRIVATE ${Boost_SYSTEM_LIBRARY}
    PRIVATE ${SQLITE_LIBRARIES}
    PRIVATE LISAAuthModule
)

install(TARGETS ${MODULE_NAME}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
