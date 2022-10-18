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

#include <memory>
#include <ostream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <type_traits>

#ifndef UNIT_TESTS
#include "Module.h"
#endif

namespace WPEFramework {
namespace Plugin {

    // operator<<'s for debugging purposes

    inline std::ostream& operator<<(std::ostream& out, std::chrono::seconds time)
    {
        return out << time.count() << " seconds";
    }

    template<class T>
    std::ostream& operator<<(std::ostream& out, std::function<T> function)
    {
        return out << "function(" << ((void*)&function) << ")";
    }

    template <class T>
    constexpr typename std::underlying_type<T>::type enumToInt(T e)
    {
        static_assert(std::is_enum<T>::value, "Given argument type is not enum");
        return static_cast<typename std::underlying_type<T>::type>(e);
    }

/** INTERNAL HELPERS *******************************************************************/

inline void lisaInternalMakeLogMessage(std::ostream&) {}

template <class A, class... Args>
void lisaInternalMakeLogMessage(std::ostream& os, A&& a, Args&&... args)
{
    lisaInternalMakeLogMessage(os << a, std::forward<Args>(args)...);
}

template <class Level, class... Args>
void lisaInternalMakeLogMessage(Args&&... args)
{
    std::ostringstream os;
    lisaInternalMakeLogMessage(os, std::forward<Args>(args)...);
    std::string str = os.str();
}

#define LOG_INTERNAL(...)  \
    std::ostringstream os; \
    lisaInternalMakeLogMessage(os, __VA_ARGS__); \
    std::string str = os.str();

/** INTERNAL HELPERS END ***************************************************************/

// use TRACE_L1 - for easier development, no need to set TRACE after every restart
//#define FORCE_TRACE_L1_DEBUGS
#ifdef FORCE_TRACE_L1_DEBUGS
#define INFO(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE_L1("%s", str.c_str()); \
    } while(0)

#define INFO_THIS(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE_L1("%s", str.c_str()); \
    } while(0)

#define ERROR(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE_L1("ERROR %s", str.c_str()); \
    } while(0)
#elif defined(TRACE_GLOBAL)
#define INFO(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE_GLOBAL(Trace::Information, ("%s", str.c_str())); \
    } while(0)

#define INFO_THIS(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE(Trace::Information, ("%s", str.c_str())); \
    } while(0)

#define ERROR(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE_GLOBAL(Trace::Error, ("%s", str.c_str())); \
    } while(0)
#else
#define INFO(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    std::cout << "INFO " << str << std::endl; \
    } while(0)

#define INFO_THIS(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    std::cout << "INFO " << str << std::endl; \
    } while(0)

#define ERROR(...) do { \
    LOG_INTERNAL(__VA_ARGS__) \
    std::cout << "ERROR " << str << std::endl; \
    } while(0)
#endif // #ifdef FORCE_TRACE_L1_DEBUGS

} // namespace Plugin
} // namespace WPEFramework

#if (__cplusplus == 201103)

namespace std
{
template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}
} // namespace std

#endif // (__cplusplus == 201103)


