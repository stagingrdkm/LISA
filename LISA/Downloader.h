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

#include "File.h"

#include <curl/curl.h>

#include <memory>
#include <chrono>
#include <functional>
#include <stdexcept>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

struct CurlDeleter
{
    void operator()(CURL* curl)
    {
        if (curl) {
            curl_easy_cleanup(curl);
        }
    }
};

class DownloadError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class Downloader
{
public:
    using ProgressListener = std::function<void(int)>;

    Downloader(const std::string& uri, ProgressListener listner);

    long getContentLength();
    void get(const Filesystem::File& destination);

private:
    void performAction();
    void doRetryWait();
    std::chrono::seconds getRetryAfterTimeSec();

    static size_t headerHandler(void* ptr, size_t size, size_t nmemb, void* userData);
    void onRetryAfter(long newRetryAfterMs);

    static int curlProgressCb(void* userData,
                              curl_off_t dltotal,
                              curl_off_t dlnow,
                              curl_off_t ultotal,
                              curl_off_t ulnow);
    bool onProgress(long dlTotal, long dlNow);

    static constexpr int HTTP_OK{200};
    static constexpr int HTTP_ACCEPTED{202};

    using CURLPtr = std::unique_ptr<CURL, CurlDeleter>;
    CURLPtr curl{nullptr};

    struct Progress {
        long total;
        long now;
        bool operator!=(const Progress& other) const {
            return (total != other.total) || (now != other.now);
        }
        int percent() const {
            return total == 0 ? 0 : (static_cast<int>((static_cast<double>(now) * 100) / total));
        }
    };
    friend std::ostream& operator<<(std::ostream& out, const Progress& progress);
    Progress progress{};

    ProgressListener progressListener{};

    // TODO read from config
    static auto constexpr DEFAULT_RETRY_AFTER{300};
    std::chrono::seconds retryAfterTime{DEFAULT_RETRY_AFTER};
};

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework

