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

#include "Downloader.h"
#include "Debug.h"

#include <cassert>
#include <thread>

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

Downloader::Downloader(const std::string& uri,
                       DownloaderListener& aListener,
                       const Config& config)
                :
                listener{aListener}
{
    static CurlLazyInitializer lazyInit;

    retryAfterTime = std::chrono::seconds(config.getDownloadRetryAfterSeconds());
    retryMaxTimes = config.getDownloadRetryMaxTimes();

    /* init the curl session */
    curl.reset(curl_easy_init());

    assert(curl && "Error initializing curl");

    curl_easy_setopt(curl.get(), CURLOPT_XFERINFOFUNCTION, curlProgressCb);
    curl_easy_setopt(curl.get(), CURLOPT_XFERINFODATA, this);
    // needed to make progress callback get called
    curl_easy_setopt(curl.get(), CURLOPT_NOPROGRESS, 0L);

    // timeout for whole operation
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT_MS, config.getDownloadTimeoutSeconds() * 1000);

    // parsing header is required "Retry-After" header
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, this);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, headerHandler);

    curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 1L);

    curl_easy_setopt(curl.get(), CURLOPT_URL, uri.c_str());

    INFO("Downloader created, uri: ", uri);
}

// returns 0 if error or content length unknown
unsigned long long Downloader::getContentLength()
{
    curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 1);

    performAction();

    curl_off_t curlContentLength{};
    auto res = curl_easy_getinfo(curl.get(), CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &curlContentLength);
    if (res == CURLE_OK && curlContentLength != -1)
      return static_cast<unsigned long long>(curlContentLength);
    else
      return 0;
}

void Downloader::get(const std::string& destination)
{
    Filesystem::File destinationFile{destination};
    INFO("downloading...");

    curl_easy_setopt(curl.get(), CURLOPT_NOBODY, 0);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, destinationFile.getHandle());

    performAction();
}

void Downloader::performAction()
{
    while(true)
    {
        CURLcode result = curl_easy_perform(curl.get());

        if (result == CURLE_ABORTED_BY_CALLBACK) {
            throw CancelledException();
        } else if (result != CURLE_OK) {
            std::string message = std::string{"download error "} + curl_easy_strerror(result);
            throw DownloadError(message);
        }

        long httpStatus{};
        curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpStatus);

        if (httpStatus == HTTP_OK) {
            break;
        } else if (httpStatus == HTTP_ACCEPTED) {
            if (retryMaxTimes > 0) {
                retryMaxTimes--;
                doRetryWait();
            } else {
                throw DownloadError("download error failed after max retries");
            }
        } else {
            std::string message = std::string{"http error "} + std::to_string(httpStatus);
            throw DownloadError(message);
        }
    }
}

void Downloader::doRetryWait()
{
    auto retryTime = getRetryAfterTimeSec();
    INFO("Retry-After received, wait time ", retryAfterTime);
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
    auto oldRetryAfterTime = retryAfterTime;
    retryAfterTime = std::chrono::seconds(newRetryAfterSec);
    INFO("Retry-After changed, old=", oldRetryAfterTime, " new=", retryAfterTime);
}

int Downloader::curlProgressCb(void* userData,
                               curl_off_t dltotal,
                               curl_off_t dlnow,
                               curl_off_t /* ultotal */,
                               curl_off_t /* ulnow */)
{
    auto downloader = static_cast<Downloader*>(userData);
    // If non-zero value is returned - abort transfer with CURLE_ABORTED_BY_CALLBACK.
    return downloader->onProgress(dltotal, dlnow);
}

bool Downloader::onProgress(long dlTotal, long dlNow)
{
    if (! listener.isCancelled()) {
        Progress newProgres = {dlTotal, dlNow};
        if (newProgres != progress) {
            progress = newProgres;
            INFO("download progress ", progress);
            listener.setProgress(progress.percent());
        }
        return false;
    } else {
        INFO("download canceled");
        return true;
    }
}

std::ostream& operator<<(std::ostream& out, const Downloader::Progress& progress)
{
    return out << progress.percent() << "% [" << progress.now << "/" << progress.total << "]";
}

} // namespace LISA
} // namespace Plugin
} // namespace WPEFramework

