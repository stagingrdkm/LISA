/*
* Copyright 2022 Liberty Global Service B.V.
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

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <chrono>
#include <thread>
#include <boost/filesystem.hpp>
#include <sqlite3.h>
#include <unistd.h>
#include <cstdlib>
#include <csignal>
#include <fstream>
#include <string>
#include <iostream>

#include <Executor.h>
#include <Filesystem.h>

using namespace std;
using namespace WPEFramework::Plugin::LISA;

#define DACAPP_ID "com.rdk.waylandegltest"
#define DACAPP_MIME "application/vnd.rdk-app.dac.native"
#define DACAPP_VERSION "1.0.0"

static string lisa_playground = "./lisa_playground";
static string db_subpath = "/apps/dac/db";
static string apps_subpath = "/apps/dac/images";
static string data_subpath = "/apps_storage/dac";
static string annotations_regex = "public\\\\.*";

#define USE_INTERNAL_TARBALL

#ifndef USE_INTERNAL_TARBALL
static string demo_tarball = "https://gitlab.com/stagingrdkm/bundles/-/raw/main/rpi3/com.libertyglobal.app.waylandegltest_3.2.1_arm_linux_rpi3_dunfell_rdk2020Q4_3.tar.gz";
#else
static string demo_tarball = "http://127.0.0.1:8899/waylandegltest.tar.gz";
static string demo_tarball2 = "http://127.0.0.1:8899/waylandegltest2.tar.gz";

class TestRunListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;
    pid_t httpserver_pid{0};
    pid_t mock_ws_pid{0};

    void testRunStarting(Catch::TestRunInfo const&) override {
        cout << "Starting simple http server..." << endl;
        pid_t pid;
        pid = fork();
        if (pid < 0)
            return;
        else if (pid == 0) {
            chdir("./files");
            execl("/usr/bin/env", "/usr/bin/env", "python3",  "-m", "http.server", "8899", (char*) NULL);
            perror("execl");
            exit(1);
        }
        sleep(1);
        httpserver_pid = pid;
    }

    void testRunEnded( Catch::TestRunStats const& ) override {
        cout << "Stopping simple http server..." << endl;
        kill(httpserver_pid, SIGTERM);
    }

    void testCaseStarting(Catch::TestCaseInfo const &info) override {
        std::string mock_name = "";
        std::for_each(info.tags.begin(), info.tags.end(), [&mock_name](Catch::Tag ttag) {
            auto tag = (std::string)ttag.original;
            auto pos = tag.find("mock=");
            if (pos != string::npos) {
                pos += 5;
                if (tag.length() > pos) {
                    mock_name = tag.substr(pos, tag.length() - pos);
                }
            }
        });

        if (!mock_name.empty()) {
            printf("STARTING MOCK: %s\n", mock_name.c_str());
            start_ws_mock(mock_name.c_str());
            sleep(1);
        }
    }

    void testCaseEnded(Catch::TestCaseStats const &) override {
        stop_ws_mock();
        sleep(1);
    }

    void stop_ws_mock() {
        if (mock_ws_pid != 0) {
            printf("Will kill mock %d\n", mock_ws_pid);
            kill(mock_ws_pid, SIGKILL);
            mock_ws_pid = 0;
        }
    }

    void start_ws_mock(const char *command) {
        pid_t pid;
        pid = fork();
        if (pid < 0)
            return;
        else if (pid == 0) {
            execl(command, command, (char *) NULL);
            perror("execl");
            exit(1);
        }
        mock_ws_pid = pid;
        printf("Mock started %d\n", mock_ws_pid);
    }

};
CATCH_REGISTER_LISTENER(TestRunListener)
#endif //USE_INTERNAL_TARBALL

static void configure(Executor &lisa, std::string annotations_file = "") {
    boost::filesystem::remove_all(lisa_playground);
    boost::filesystem::create_directories(lisa_playground);
    lisa_playground = boost::filesystem::canonical(lisa_playground).string();
    lisa.Configure(" { \"dbpath\":\""   + lisa_playground + db_subpath   + "\","
                   "   \"appspath\":\"" + lisa_playground + apps_subpath + "\","
                   "   \"datapath\":\"" + lisa_playground + data_subpath + "\","
                   "   \"annotationsFile\":\"" + annotations_file + "\","
                   "   \"annotationsRegex\":\"" + annotations_regex + "\","
                   "   \"downloadRetryAfterSeconds\":" + std::to_string(10) + ","
                   "   \"downloadRetryMaxTimes\":" +  std::to_string(1) + ","
                   "   \"downloadTimeoutSeconds\":" +  std::to_string(30) + "}"
                   );
}

static Executor::OperationStatusEvent last_event_received_;
static vector<Executor::OperationStatusEvent> all_events_received_;
static bool record_all_events_{false};
static condition_variable cond_var_;
static mutex mutex_;
static bool event_received_;

static  void eventHandler(const Executor::OperationStatusEvent &event) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (record_all_events_) {
        all_events_received_.push_back(event);
    }
    if (event.status == Executor::OperationStatus::PROGRESS) {
        // ignore progress
        return;
    }
    cout << "Received event from " << event.id << " : " << event.operationStr() << ":" << event.statusStr() << endl;
    last_event_received_ = event;
    event_received_ = true;
    cond_var_.notify_one();
}

static bool waitForEvent(int timeout_secs) {
    std::unique_lock<std::mutex> lock(mutex_);
    event_received_ = false;
    const auto status =
            cond_var_.wait_for(
                    lock, std::chrono::seconds(timeout_secs), [] { return event_received_; });
    return status;
}

static bool waitForEventIncludingAlreadyArrivedOne(int timeout_secs) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (event_received_) {
        event_received_ = false;
        return true;
    }
    const auto status =
            cond_var_.wait_for(
                    lock, std::chrono::seconds(timeout_secs), [] { return event_received_; });
    return status;
}

static int countInDB(string tablename) {
    sqlite3* sqlite;
    string db_path = lisa_playground + db_subpath + "/0/apps.db";
    auto rc = sqlite3_open(db_path.c_str(), &sqlite);
    if (rc) {
        cout << "Cannot open DB!! " << endl;
        return -1;
    }

    int result = 0;
    std::string query = "select count(*) from "s+tablename+";";
    sqlite3_stmt* stmt;
    sqlite3_prepare_v2(sqlite, query.c_str(), query.length(), &stmt, nullptr);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(sqlite);
    return result;
}

static int countAppsInDB() {
    return countInDB("apps");
}

static int countInstalledAppsInDB() {
    return countInDB("installed_apps");
}

static bool findPathInAppsPath(string path) {
    boost::filesystem::recursive_directory_iterator dir(lisa_playground + apps_subpath);
    string pathToFind = lisa_playground + apps_subpath + "/" + path;
    for (auto &&i: dir) {
        if (i == pathToFind)
            return true;
    }
    return false;
}

static bool findPathInStoragePath(string path) {
    boost::filesystem::recursive_directory_iterator dir(lisa_playground + data_subpath);
    string pathToFind = lisa_playground + data_subpath + "/" + path;
    for (auto &&i: dir) {
        if (i == pathToFind)
            return true;
    }
    return false;
}

/*******************************************************************************************/
// Test scenarios
/*******************************************************************************************/
CATCH_TEST_CASE("LISA : install app", "[all][test1][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 1);
    CATCH_CHECK(findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));

    // check error code in case of app not found
    result = lisa.GetStorageDetails(DACAPP_MIME, "invalid", DACAPP_VERSION, details);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);
}

CATCH_TEST_CASE("LISA : install 2 apps", "[all][test2][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install(DACAPP_MIME, "com.rdk.waylandegltest2", DACAPP_VERSION, demo_tarball, "appname2", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest2");
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails(DACAPP_MIME, "com.rdk.waylandegltest2", DACAPP_VERSION, details2);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details2.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest2/1.0.0/"));
    CATCH_CHECK(details2.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest2/"));

    CATCH_CHECK(countAppsInDB() == 2);
    CATCH_CHECK(countInstalledAppsInDB() == 2);
}

CATCH_TEST_CASE("LISA : install apps, 2 versions", "[all][test3][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install(DACAPP_MIME, DACAPP_ID, "2.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == "2.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "2.0.0", details2);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details2.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/2.0.0/"));
    CATCH_CHECK(details2.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 2);

    std::vector<DataStorage::AppDetails> appDetails;
    result = lisa.GetAppDetailsList(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "", "", appDetails);
    CATCH_CHECK(appDetails.size() == 1);
    std::vector<DataStorage::AppDetails> appDetails2;
    result = lisa.GetAppDetailsList(DACAPP_MIME, DACAPP_ID, "", "", "", appDetails2);
    CATCH_CHECK(appDetails2.size() == 2);
}

CATCH_TEST_CASE("LISA : install 2 apps same id, type and version = not allowed", "[all][test4][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_ALREADY_INSTALLED);
}

CATCH_TEST_CASE("LISA : install 2 apps same id but different type = not allowed", "[all][test5][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install("application/vnd.rdk-app.dac-other.native", DACAPP_ID, "2.0.0",
                          demo_tarball, "appname2", "cat", handle);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac-other.native", "com.rdk.waylandegltest2", DACAPP_VERSION, details2);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details2.appPath.empty());
    CATCH_CHECK(details2.persistentPath.empty());
}

CATCH_TEST_CASE("LISA : install apps, 2 versions, remove version 1", "[all][test6][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install(DACAPP_MIME, DACAPP_ID, "2.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == "2.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "2.0.0", details2);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details2.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/2.0.0/"));
    CATCH_CHECK(details2.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 2);

    result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "full", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details3;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "2.0.0", details3);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details3.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/2.0.0/"));
    CATCH_CHECK(details3.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 1);
    CATCH_CHECK(findPathInAppsPath("0/com.rdk.waylandegltest/2.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));

    // uninstall remaining app
    result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, "2.0.0", "full", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    CATCH_CHECK(countAppsInDB() == 0);
    CATCH_CHECK(countInstalledAppsInDB() == 0);
    CATCH_CHECK(!findPathInAppsPath("0/com.rdk.waylandegltest/2.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(!findPathInStoragePath("0/com.rdk.waylandegltest"));
}

CATCH_TEST_CASE("LISA : lock/unlock test", "[all][test7][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    result = lisa.Lock(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "some reason", "me" , handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());

    string otherhandle;
    result = lisa.Lock(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "some reason2", "me2" , otherhandle);
    CATCH_CHECK(result == Executor::ReturnCodes::ERROR_APP_LOCKED);

    result = lisa.Lock(DACAPP_MIME, "com.rdk.waylandegltest2", DACAPP_VERSION, "some reason3", "me3" , otherhandle);
    CATCH_CHECK(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);

    string reason, who;
    result = lisa.GetLockInfo(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, reason, who);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(reason == "some reason");
    CATCH_CHECK(who == "me");

    reason = "";
    who = "";
    result = lisa.GetLockInfo(DACAPP_MIME, "com.rdk.waylandegltest2", DACAPP_VERSION, reason, who);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);
    CATCH_CHECK(reason.empty());
    CATCH_CHECK(who.empty());

    result = lisa.Unlock(handle);
    CATCH_CHECK(result == 0);

    result = lisa.GetLockInfo(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, reason, who);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_HANDLE);
    CATCH_CHECK(reason.empty());
    CATCH_CHECK(who.empty());

    result = lisa.Unlock(handle);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_HANDLE);
}

CATCH_TEST_CASE("LISA : lock/uninstall test normal", "[all][test8][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    result = lisa.Lock(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "some reason", "me" , handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());

    string uninstall_handle;
    result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "full" , uninstall_handle);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_APP_LOCKED);

    result = lisa.Unlock(handle);
    CATCH_REQUIRE(result == 0);

    result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "full" , uninstall_handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(uninstall_handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
}

CATCH_TEST_CASE("LISA : uninstall test (upgrade)", "[all][test9][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    string uninstall_handle;
    result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "upgrade" , uninstall_handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(uninstall_handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 0);
    CATCH_CHECK(!findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));

    Filesystem::StorageDetails details, detailsb;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "", details);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(details.appPath.empty());
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    result = lisa.GetStorageDetails("", DACAPP_ID, "", detailsb);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(detailsb.appPath.empty());
    CATCH_CHECK(detailsb.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    std::vector<DataStorage::AppDetails> appDetails, appDetailsb;
    result = lisa.GetAppDetailsList(DACAPP_MIME, DACAPP_ID, "", "", "", appDetails);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(appDetails.size() == 1);
    result = lisa.GetAppDetailsList("", DACAPP_ID, "", "", "", appDetailsb);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(appDetailsb.size() == 1);

    result = lisa.Install(DACAPP_MIME, DACAPP_ID, "2.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 1);

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "2.0.0", details2);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(details2.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/2.0.0/"));
    CATCH_CHECK(details2.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    appDetails.clear();
    result = lisa.GetAppDetailsList(DACAPP_MIME, DACAPP_ID, "", "", "", appDetails);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(appDetails.size() == 1);
}

CATCH_TEST_CASE("LISA : uninstall test (upgrade), followed by full uninstall", "[all][test10][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    CATCH_SECTION( "normal case: uninstall  (upgrade) followed by full uninstall" ) {
        string uninstall_handle;
        result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "upgrade",
                                uninstall_handle);
        CATCH_REQUIRE(result == 0);
        CATCH_REQUIRE_FALSE(uninstall_handle.empty());
        CATCH_REQUIRE(waitForEvent(30));
        CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
        CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

        CATCH_CHECK(countAppsInDB() == 1);
        CATCH_CHECK(countInstalledAppsInDB() == 0);
        CATCH_CHECK(!findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
        CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));

        Filesystem::StorageDetails details;
        result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "", details);
        CATCH_CHECK(result == 0);
        CATCH_CHECK(details.appPath.empty());
        CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

        std::vector<DataStorage::AppDetails> appDetails;
        result = lisa.GetAppDetailsList(DACAPP_MIME, DACAPP_ID, "", "", "",
                                        appDetails);
        CATCH_CHECK(result == 0);
        CATCH_CHECK(appDetails.size() == 1);

        uninstall_handle = "";
        result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, "", "full",
                                uninstall_handle);
        CATCH_REQUIRE(result == 0);
        CATCH_REQUIRE_FALSE(uninstall_handle.empty());
        CATCH_REQUIRE(waitForEvent(30));
        CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
        CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

        CATCH_CHECK(countAppsInDB() == 0);
        CATCH_CHECK(countInstalledAppsInDB() == 0);
        CATCH_CHECK(!findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
        CATCH_CHECK(!findPathInStoragePath("0/com.rdk.waylandegltest"));

        Filesystem::StorageDetails details2;
        result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "", details2);
        CATCH_CHECK(result == 0);
        CATCH_CHECK(details2.appPath.empty());
        CATCH_CHECK(details2.persistentPath.empty());

        std::vector<DataStorage::AppDetails> appDetails2;
        result = lisa.GetAppDetailsList(DACAPP_MIME, DACAPP_ID, "", "", "",
                                        appDetails2);
        CATCH_CHECK(result == 0);
        CATCH_CHECK(appDetails2.size() == 0);
    }

    CATCH_SECTION( "special case: trying full uninstall without version specified, when app still installed" ) {
        string uninstall_handle = "";
        result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, "", "full",
                                uninstall_handle);
        CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);

        CATCH_CHECK(countAppsInDB() == 1);
        CATCH_CHECK(countInstalledAppsInDB() == 1);
        CATCH_CHECK(findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
        CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));
    }
}

void outputFile(string path, string contents) {
    ofstream out(path);
    out << contents;
    out.close();
}

string inputFile(string path) {
    ifstream t(path);
    return string((istreambuf_iterator<char>(t)), istreambuf_iterator<char>());
}

CATCH_TEST_CASE("LISA : uninstall test (upgrade), followed by install new version same app", "[all][test11][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, "", details);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(details.appPath.empty());
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    // simulate app was run and persisted some data
    string persisted_data{"Some persisted data here..."};
    outputFile(details.persistentPath+"somedata.txt", persisted_data);

    string uninstall_handle;
    result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "upgrade" , uninstall_handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(uninstall_handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 0);
    CATCH_CHECK(!findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));
    CATCH_CHECK(inputFile(details.persistentPath+"somedata.txt") == persisted_data);

    std::vector<DataStorage::AppDetails> appDetails;
    result = lisa.GetAppDetailsList(DACAPP_MIME, DACAPP_ID, "", "", "", appDetails);
    CATCH_CHECK(result == 0);
    CATCH_CHECK(appDetails.size() == 1);

    result = lisa.Install(DACAPP_MIME, DACAPP_ID, "2.0.0", demo_tarball,
                               "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 1);
    CATCH_CHECK(findPathInAppsPath("0/com.rdk.waylandegltest/2.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));
    CATCH_CHECK(inputFile(details.persistentPath+"somedata.txt") == persisted_data);
}

bool findInMetadata(const DataStorage::AppMetadata &metadata, string key, string val) {
    for(const auto &x: metadata.metadata) {
        if (x.first == key) {
            return x.second == val;
        }
    }
    return false;
}

CATCH_TEST_CASE("LISA : metadata test", "[all][test12][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball,
                               "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    {
        DataStorage::AppMetadata metadata;
        result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, metadata);
        CATCH_REQUIRE(result == 0);
        CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
        CATCH_CHECK(metadata.appDetails.version == DACAPP_VERSION);
        CATCH_CHECK(metadata.metadata.size() == 0);
    }

    result = lisa.SetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "key1", "value1");
    CATCH_REQUIRE(result == 0);

    {
        DataStorage::AppMetadata metadata;
        result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, metadata);
        CATCH_REQUIRE(result == 0);
        CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
        CATCH_CHECK(metadata.appDetails.version == DACAPP_VERSION);
        CATCH_CHECK(metadata.metadata.size() == 1);
        CATCH_CHECK(findInMetadata(metadata, "key1", "value1"));
    }

    result = lisa.Install(DACAPP_MIME, DACAPP_ID, "2.0.0", demo_tarball,
                          "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    result = lisa.SetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "key2", "value2");
    CATCH_REQUIRE(result == 0);

    result = lisa.SetMetadata(DACAPP_MIME, DACAPP_ID, "2.0.0", "key3", "value3");
    CATCH_REQUIRE(result == 0);

    CATCH_SECTION( "clear metadata all at once" ) {
        result = lisa.ClearMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "");
        CATCH_REQUIRE(result == 0);

        {
            DataStorage::AppMetadata metadata;
            result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION,
                                      metadata);
            CATCH_REQUIRE(result == 0);
            CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
            CATCH_CHECK(metadata.appDetails.version == DACAPP_VERSION);
            CATCH_CHECK(metadata.metadata.size() == 0);
        }

        // the one key for 2.0.0 should still be there
        CATCH_CHECK(countInDB("metadata") == 1);
    }

    CATCH_SECTION( "metadata replace key test" ) {
        result = lisa.SetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "key1",
                                  "value2");
        CATCH_REQUIRE(result == 0);

        result = lisa.SetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "key1",
                                  "valuex");
        CATCH_REQUIRE(result == 0);

        {
            DataStorage::AppMetadata metadata;
            result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, metadata);
            CATCH_REQUIRE(result == 0);
            CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
            CATCH_CHECK(metadata.appDetails.version == DACAPP_VERSION);
            CATCH_CHECK(metadata.metadata.size() == 2);
            CATCH_CHECK(findInMetadata(metadata, "key1", "valuex"));
            CATCH_CHECK(findInMetadata(metadata, "key2", "value2"));
        }

        CATCH_CHECK(countInDB("metadata") == 3);
    }

    CATCH_SECTION( "normal metadata test + uninstall that should remove all" ) {
        {
            DataStorage::AppMetadata metadata;
            result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION,
                                      metadata);
            CATCH_REQUIRE(result == 0);
            CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
            CATCH_CHECK(metadata.appDetails.version == DACAPP_VERSION);
            CATCH_CHECK(metadata.metadata.size() == 2);
            CATCH_CHECK(findInMetadata(metadata, "key1", "value1"));
            CATCH_CHECK(findInMetadata(metadata, "key2", "value2"));
        }

        {
            DataStorage::AppMetadata metadata;
            result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, "2.0.0",
                                      metadata);
            CATCH_REQUIRE(result == 0);
            CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
            CATCH_CHECK(metadata.appDetails.version == "2.0.0");
            CATCH_CHECK(metadata.metadata.size() == 1);
            CATCH_CHECK(findInMetadata(metadata, "key3", "value3"));
        }

        result = lisa.ClearMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "key1");
        CATCH_REQUIRE(result == 0);

        result = lisa.ClearMetadata(DACAPP_MIME, DACAPP_ID, "2.0.0", "key3");
        CATCH_REQUIRE(result == 0);

        {
            DataStorage::AppMetadata metadata;
            result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION,
                                      metadata);
            CATCH_REQUIRE(result == 0);
            CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
            CATCH_CHECK(metadata.appDetails.version == DACAPP_VERSION);
            CATCH_CHECK(metadata.metadata.size() == 1);
            CATCH_CHECK(findInMetadata(metadata, "key2", "value2"));
        }

        {
            DataStorage::AppMetadata metadata;
            result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, "2.0.0",
                                      metadata);
            CATCH_REQUIRE(result == 0);
            CATCH_CHECK(metadata.appDetails.id == DACAPP_ID);
            CATCH_CHECK(metadata.appDetails.version == "2.0.0");
            CATCH_CHECK(metadata.metadata.size() == 0);
        }

        string uninstall_handle;
        result = lisa.Uninstall(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, "full",
                                uninstall_handle);
        CATCH_REQUIRE(result == 0);
        CATCH_REQUIRE_FALSE(uninstall_handle.empty());
        CATCH_REQUIRE(waitForEvent(30));
        CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
        CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

        {
            DataStorage::AppMetadata metadata;
            result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION,
                                      metadata);
            CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_GENERAL);
            CATCH_CHECK(metadata.metadata.size() == 0);
        }

        CATCH_CHECK(countInDB("metadata") == 0);
    }
}

CATCH_TEST_CASE("LISA : cancel installation test", "[all][test13][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball,
                               "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());

    result = lisa.Cancel(handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE(waitForEventIncludingAlreadyArrivedOne(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::CANCELLED);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, details);
    CATCH_CHECK(result == 0);

    CATCH_CHECK(countAppsInDB() == 0);
    CATCH_CHECK(countInstalledAppsInDB() == 0);
    CATCH_CHECK(!findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(!findPathInStoragePath("0/com.rdk.waylandegltest"));
}

CATCH_TEST_CASE("LISA : basic install progress test", "[all][test14][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball,
                               "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    record_all_events_ = true;

    uint32_t progress = -1;
    result = lisa.GetProgress(handle, progress);
    cout << progress << endl;
    CATCH_CHECK(result == 0);
    CATCH_CHECK(progress >= 0);

    CATCH_CHECK(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    int cnt = 0;
    for(const auto& x: all_events_received_) {
        if (x.status == Executor::OperationStatus::PROGRESS) {
            cnt++;
        }
    }
    CATCH_CHECK(cnt > 0);

    record_all_events_ = false;
}

CATCH_TEST_CASE("LISA : verify annotations installed with app", "[all][test15][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa, "config.json");

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    DataStorage::AppMetadata metadata;
    result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, metadata);
    CATCH_REQUIRE(result == 0);
    bool found_requires_ocdm = false;
    bool found_requires_rialto = false;
    for(const auto& x: metadata.metadata) {
        if (x.first == "public.requires.ocdm" && x.second == "1") {
            found_requires_ocdm = true;
        } else if (x.first == "public.requires.rialto" && x.second == "1") {
            found_requires_rialto = true;
        }
    }
    CATCH_CHECK(found_requires_ocdm);
    CATCH_CHECK(found_requires_rialto);
    CATCH_CHECK(metadata.metadata.size() == 2);
}

CATCH_TEST_CASE("LISA : verify annotations installed with app, other annotations file", "[all][test16][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa, "annotations.json");

    string handle;
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball2, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);

    DataStorage::AppMetadata metadata;
    result = lisa.GetMetadata(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, metadata);
    CATCH_REQUIRE(result == 0);
    bool found_requires_ocdm = false;
    bool found_requires_rialto = false;
    for(const auto& x: metadata.metadata) {
        if (x.first == "public.requires.ocdm" && x.second == "YES") {
            found_requires_ocdm = true;
        } else if (x.first == "public.requires.rialto" && x.second == "YES") {
            found_requires_rialto = true;
        }
    }
    CATCH_CHECK(found_requires_ocdm);
    CATCH_CHECK(found_requires_rialto);
    CATCH_CHECK(metadata.metadata.size() == 2);
}

CATCH_TEST_CASE("LISA : test of downloadRetryAfterSeconds and downloadRetryMaxTimes", "[all][test17][slow][mock=server202.py]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    string demo_tarball_202 = "http://127.0.0.1:8898/waylandegltest.tar.gz";
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball_202, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());

    // should not expect too fast response
    CATCH_REQUIRE(!waitForEvent(5));
    // now there should be a response
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::FAILED);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);
}

CATCH_TEST_CASE("LISA : test of downloadTimeoutSeconds", "[all][test18][slow][mock=servertimeout.py]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    string demo_tarball_timeout = "http://127.0.0.1:8897/waylandegltest.tar.gz";
    auto result = lisa.Install(DACAPP_MIME, DACAPP_ID, DACAPP_VERSION, demo_tarball_timeout, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());

    // should not expect too fast response
    CATCH_REQUIRE(!waitForEvent(15));
    // now there should be a response
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::FAILED);
    CATCH_CHECK(last_event_received_.id == DACAPP_ID);
    CATCH_CHECK(last_event_received_.type == DACAPP_MIME);
    CATCH_CHECK(last_event_received_.version == DACAPP_VERSION);
    CATCH_CHECK(last_event_received_.handle == handle);
}
