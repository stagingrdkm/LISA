/*****************************************************************************
* Copyright © 2020 Liberty Global B.V. and its Licensors.
* All rights reserved.
* Licensed by RDK Management, LLC under the terms of the RDK license.
* ============================================================================
* Liberty Global B.V. CONFIDENTIAL AND PROPRIETARY
* ============================================================================
* This file (and its contents) are the intellectual property of Liberty Global B.V.
* It may not be used, copied, distributed or otherwise disclosed in whole or in
* part without the express written permission of Liberty Global B.V.
* The RDK License agreement constitutes express written consent by Liberty Global.
* ============================================================================
* This software is the confidential and proprietary information of Liberty Global B.V.
* ("Confidential Information"). You shall not disclose this source code or
* such Confidential Information and shall use it only in accordance with the
* terms of the license agreement you entered into.
*
* LIBERTY GLOBAL B.V. MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE
* SUITABILITY OF THE SOFTWARE, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT
* LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE, OR NON-INFRINGEMENT. LIBERTY GLOBAL B.V. SHALL NOT BE LIABLE FOR
* ANY DAMAGES SUFFERED BY LICENSEE NOR SHALL THEY BE RESPONSIBLE AS A RESULT
* OF USING, MODIFYING OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.
******************************************************************************/

#pragma once

#include <ostream>
#include <type_traits>

#include "Module.h"

namespace WPEFramework {
namespace Plugin {
namespace LISA {

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


#define INFO(...) { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE_GLOBAL(Trace::Information, ("%s", str.c_str())); \
}

#define INFO_THIS(...) { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE(Trace::Information, ("%s", str.c_str())); \
}

#define ERROR(...) { \
    LOG_INTERNAL(__VA_ARGS__) \
    TRACE_GLOBAL(Trace::Error, ("%s", str.c_str())); \
}

// operator<<'s for debugging purposes

inline std::ostream& operator<<(std::ostream& out, std::chrono::seconds time)
{
    return out << time << " seconds";
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

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework


