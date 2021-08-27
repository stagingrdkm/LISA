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

#include "Downloader.h"

#include <cassert>

namespace WPEFramework {
namespace Plugin {
namespace LISA {

namespace { // anonymous
struct CurlLazyInitializer
{
    CurlLazyInitializer()
    {
        curl_global_init(CURL_GLOBAL_ALL);
    }
};

} // namespace anonymous

Downloader::Downloader(const std::string& uri)
{
    static CurlLazyInitializer lazyInit;

    /* init the curl session */
    curl.reset(curl_easy_init());

    assert(curl && "Error initializing curl");

    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, curlProgressCb);
    // needed to make progress callback get called
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);

    // TODO read from config
    auto constexpr DEFAULT_TIMEOUT_MS = 15 * 60 * 1000;
    // timeout for whole operation
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, DEFAULT_TIMEOUT_MS);

    // parsing header is required "Retry-After" header
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, headerHandler);

    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);

    curl_easy_setopt(curl.get(), CURLOPT_URL, uri.c_str());

    TRACE_L1("Downloader created, uri: %s", uri.c_str());
}

long Downloader::getContentLength()
{
    curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1);

    performAction();

    curl_off_t curlContentLength{};
    curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &curlContentLength);

    return static_cast<long>(curlContentLength);
}

void  Downloader::get(const Filesystem::File& destination)
{
    TRACE_L1("downloading...");

    curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, destination.getHandle());

    performAction();
}

void Downloader::performAction()
{
    while(true)
    {
        CURLcode result = curl_easy_perform(curl.get());

        if (result != CURLE_OK) {
            std::string message = std::string{"download error "} + curl_easy_strerror(result);
            throw DownloadError(message);
        }

        long httpStatus{};
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpStatus);

        if (httpStatus == HTTP_OK) {
            break;
        } else if (httpStatus == HTTP_ACCEPTED) {
            doRetryWait();
        } else {
            std::string message = std::string{"http error "} + std::to_string(httpStatus);
            throw DownloadError(message);
        }
    }
}

void Downloader::doRetryWait()
{
    auto retryTime = getRetryAfterTimeSec();
    TRACE_L1("Retry-After received, wait time %d sec", static_cast<int>(retryTime.count()));
    std::this_thread::sleep_for(retryTime);
}

std::chrono::seconds Downloader::getRetryAfterTimeSec()
{
//    curl_off_t waitTime{DEFAULT_RETRY_IN};
//    TODO default onemw curl - 7.59 - does not yet support this option, it was added in 7.66 -
//          either use separate version of curl like ipplayer or parse header manually
//    auto result = curl_easy_getinfo(curl.get(), CURLINFO_RETRY_AFTER, &waitTime);
//    retryAfterMs = static_cast<int>(waitTime);

    return retryAfterTime;
}

size_t Downloader::headerHandler(void* ptr, size_t size, size_t nmemb, void* userData)
{
    std::string headerLine{static_cast<char*>(ptr), static_cast<char*>(ptr) + nmemb};

    const std::string retryAfterPrefix{"Retry-After:"};
    auto pos = headerLine.find(retryAfterPrefix) ;

    if (pos != std::string::npos) {
        pos += retryAfterPrefix.length();
        // TODO add parsing Date
        auto parsedValue{0};
        try {
            parsedValue= std::stoi(headerLine.substr(pos));
        }
        catch(...){
            // noop
        }

        if (parsedValue >= 0) {
            auto downloader = static_cast<Downloader*>(userData);
            downloader->onRetryAfter(parsedValue);
        }
    }
    return size * nmemb;
}

void Downloader::onRetryAfter(long newRetryAfterSec)
{
    TRACE_L1("Retry-After changed, old %d new %ld", static_cast<int>(retryAfterTime.count()), newRetryAfterSec);
    retryAfterTime = std::chrono::seconds(newRetryAfterSec);
}

int Downloader::curlProgressCb(void* userData,
                               curl_off_t dltotal,
                               curl_off_t dlnow,
                               curl_off_t ultotal,
                               curl_off_t ulnow)
{
    auto downloader = static_cast<Downloader*>(userData);
    // If non-zero value is returned - abort transfer with CURLE_ABORTED_BY_CALLBACK.
    return downloader->onProgress(dltotal, dlnow);
}

bool Downloader::onProgress(long dlTotal, long dlNow)
{
    TRACE_L1("downloaded: %ld/%ld", dlNow, dlTotal);
    // TODO progress notification
    return false;
}

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework

