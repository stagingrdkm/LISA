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

#include "File.h"
#include "Config.h"

#include <curl/curl.h>

#include <functional>
#include <memory>
#include <chrono>
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

class CancelledException : public std::exception
{
public:
    using std::exception::exception;
};

class DownloaderListener
{
public:
    virtual ~DownloaderListener() = default;

    virtual void setProgress(int progress) = 0;
    virtual bool isCancelled() = 0;
};

class Downloader
{
public:
    Downloader(const std::string& uri, DownloaderListener& aListener,
               const Config& config);
    Downloader(const Downloader& other) = delete;
    Downloader& operator=(const Downloader& other) = delete;

    // returns 0 if error or content length  unknown
    unsigned long long getContentLength();
    void get(const std::string& destination);

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
    friend std::ostream& operator<<(std::ostream& out, const Progress& aProgress);
    Progress progress{};

    DownloaderListener& listener;

    std::chrono::seconds retryAfterTime{300};
    unsigned int retryMaxTimes;
};

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework

