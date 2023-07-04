# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2022 Liberty Global Service B.V.
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

cmake_minimum_required(VERSION 3.5)
project(lisa_tests)
find_package(PkgConfig)
find_package(Python COMPONENTS Interpreter REQUIRED)

set(CMAKE_CXX_STANDARD 14)
set(CXX_STANDARD_REQUIRED YES)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -g -O0")

#add_definitions(-DLISA_APPS_GID=0)
#add_definitions(-DLISA_DATA_GID=0)
add_definitions(-DUNIT_TESTS -DCATCH_CONFIG_PREFIX_ALL -DCATCH_CONFIG_EXTERNAL_INTERFACES)

include_directories(. ../)
set(LibArchive_INCLUDE_DIR "/usr/local/opt/libarchive/include")

find_package(CURL REQUIRED)
find_package(LibArchive REQUIRED)
find_package(Boost COMPONENTS system filesystem REQUIRED)
pkg_search_module(SQLITE REQUIRED sqlite3)
find_package(Catch2 3 REQUIRED)

include_directories(
        ${SQLITE_INCLUDE_DIRS}
        ${Boost_INCLUDE_DIRS}
        ${LibArchive_INCLUDE_DIR}
        ${Catch2_INCLUDE_DIR}
)

set(SOURCE_FILES
        LISATests.cpp
        ../AuthModule/AuthStub.c
        ../Archives.cpp
        ../Config.cpp
        ../Downloader.cpp
        ../Executor.cpp
        ../File.cpp
        ../Filesystem.cpp
        ../SqlDataStorage.cpp
        )

add_executable(lisa_test ${SOURCE_FILES})
target_link_libraries(lisa_test
        PRIVATE ${CURL_LIBRARIES}
        PRIVATE ${LibArchive_LIBRARIES}
        PRIVATE ${Boost_FILESYSTEM_LIBRARY}
        PRIVATE ${Boost_SYSTEM_LIBRARY}
        PRIVATE ${SQLITE_LIBRARIES}
        PRIVATE Catch2::Catch2WithMain
        )

add_custom_command(
        TARGET lisa_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_CURRENT_BINARY_DIR}/files)

add_custom_command(
        TARGET lisa_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/files/*.tar.gz
        ${CMAKE_CURRENT_BINARY_DIR}/files/)

add_custom_command(
        TARGET lisa_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/mock/*.py
        ${CMAKE_CURRENT_BINARY_DIR}/)