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
#include <chrono>
#include <thread>
#include <boost/filesystem.hpp>
#include <sqlite3.h>
#include <unistd.h>
#include <cstdlib>
#include <csignal>

#include <Executor.h>
#include <Filesystem.h>

using namespace std;
using namespace WPEFramework::Plugin::LISA;

static string lisa_playground = "./lisa_playground";
static string db_subpath = "/apps/dac/db";
static string apps_subpath = "/apps/dac/images";
static string data_subpath = "/apps_storage/dac";

#define USE_INTERNAL_TARBALL

#ifndef USE_INTERNAL_TARBALL
static string demo_tarball = "https://gitlab.com/stagingrdkm/bundles/-/raw/main/rpi3/com.libertyglobal.app.waylandegltest_3.2.1_arm_linux_rpi3_dunfell_rdk2020Q4_3.tar.gz";
#else
static string demo_tarball = "http://127.0.0.1:8899/waylandegltest.tar.gz";
class TestRunListener : public Catch::EventListenerBase {
public:
    using Catch::EventListenerBase::EventListenerBase;
    pid_t httpserver_pid;

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
};
CATCH_REGISTER_LISTENER(TestRunListener)
#endif //USE_INTERNAL_TARBALL

static void configure(Executor &lisa) {
    boost::filesystem::remove_all(lisa_playground);
    boost::filesystem::create_directories(lisa_playground);
    lisa_playground = boost::filesystem::canonical(lisa_playground).string();
    lisa.Configure(" { \"dbpath\":\""   + lisa_playground + db_subpath   + "\","
                   "   \"appspath\":\"" + lisa_playground + apps_subpath + "\","
                   "   \"datapath\":\"" + lisa_playground + data_subpath + "\" }");
}

static Executor::OperationStatusEvent last_event_received_;
static condition_variable cond_var_;
static mutex mutex_;
static bool event_received_;

static  void eventHandler(const Executor::OperationStatusEvent &event) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (event.status == Executor::OperationStatus::PROGRESS) {
        // ignore progress
        return;
    }
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
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 1);
    CATCH_CHECK(findPathInAppsPath("0/com.rdk.waylandegltest/1.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));
}

CATCH_TEST_CASE("LISA : install 2 apps", "[all][test2][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest2", "1.0.0", demo_tarball, "appname2", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest2");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest2", "1.0.0", details2);
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
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "2.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "2.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "2.0.0", details2);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details2.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/2.0.0/"));
    CATCH_CHECK(details2.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 2);

    std::vector<DataStorage::AppDetails> appDetails;
    result = lisa.GetAppDetailsList("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", "", "", appDetails);
    CATCH_CHECK(appDetails.size() == 1);
    std::vector<DataStorage::AppDetails> appDetails2;
    result = lisa.GetAppDetailsList("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "", "", "", appDetails2);
    CATCH_CHECK(appDetails2.size() == 2);
}

CATCH_TEST_CASE("LISA : install 2 apps same id, type and version = not allowed", "[all][test4][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_ALREADY_INSTALLED);
}

CATCH_TEST_CASE("LISA : install 2 apps same id but different type = not allowed", "[all][test5][quick]") {
    Executor lisa([](const Executor::OperationStatusEvent &event) {
        eventHandler(event);
    });
    configure(lisa);

    string handle;
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install("application/vnd.rdk-app.dac-other.native", "com.rdk.waylandegltest", "2.0.0",
                          demo_tarball, "appname2", "cat", handle);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac-other.native", "com.rdk.waylandegltest2", "1.0.0", details2);
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
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "2.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::INSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "2.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", details);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/1.0.0/"));
    CATCH_CHECK(details.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    Filesystem::StorageDetails details2;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "2.0.0", details2);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details2.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/2.0.0/"));
    CATCH_CHECK(details2.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 2);

    result = lisa.Uninstall("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", "full", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
    CATCH_CHECK(last_event_received_.id == "com.rdk.waylandegltest");
    CATCH_CHECK(last_event_received_.type == "application/vnd.rdk-app.dac.native");
    CATCH_CHECK(last_event_received_.version == "1.0.0");
    CATCH_CHECK(last_event_received_.handle == handle);

    Filesystem::StorageDetails details3;
    result = lisa.GetStorageDetails("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "2.0.0", details3);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(details3.appPath == (lisa_playground + apps_subpath + "/0/com.rdk.waylandegltest/2.0.0/"));
    CATCH_CHECK(details3.persistentPath == (lisa_playground + data_subpath + "/0/com.rdk.waylandegltest/"));

    CATCH_CHECK(countAppsInDB() == 1);
    CATCH_CHECK(countInstalledAppsInDB() == 1);
    CATCH_CHECK(findPathInAppsPath("0/com.rdk.waylandegltest/2.0.0/rootfs/usr/bin/wayland-egl-test"));
    CATCH_CHECK(findPathInStoragePath("0/com.rdk.waylandegltest"));

    // uninstall remaining app
    result = lisa.Uninstall("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "2.0.0", "full", handle);
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
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    result = lisa.Lock("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", "some reason", "me" , handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());

    string otherhandle;
    result = lisa.Lock("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", "some reason2", "me2" , otherhandle);
    CATCH_CHECK(result == Executor::ReturnCodes::ERROR_APP_LOCKED);

    result = lisa.Lock("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest2", "1.0.0", "some reason3", "me3" , otherhandle);
    CATCH_CHECK(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);

    string reason, who;
    result = lisa.GetLockInfo("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", reason, who);
    CATCH_REQUIRE(result == 0);
    CATCH_CHECK(reason == "some reason");
    CATCH_CHECK(who == "me");

    reason = "";
    who = "";
    result = lisa.GetLockInfo("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest2", "1.0.0", reason, who);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_WRONG_PARAMS);
    CATCH_CHECK(reason.empty());
    CATCH_CHECK(who.empty());

    result = lisa.Unlock(handle);
    CATCH_CHECK(result == 0);

    result = lisa.GetLockInfo("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", reason, who);
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
    auto result = lisa.Install("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", demo_tarball, "appname", "cat", handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);

    result = lisa.Lock("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", "some reason", "me" , handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(handle.empty());

    string uninstall_handle;
    result = lisa.Uninstall("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", "full" , uninstall_handle);
    CATCH_REQUIRE(result == Executor::ReturnCodes::ERROR_APP_LOCKED);

    result = lisa.Unlock(handle);
    CATCH_REQUIRE(result == 0);

    result = lisa.Uninstall("application/vnd.rdk-app.dac.native", "com.rdk.waylandegltest", "1.0.0", "full" , uninstall_handle);
    CATCH_REQUIRE(result == 0);
    CATCH_REQUIRE_FALSE(uninstall_handle.empty());
    CATCH_REQUIRE(waitForEvent(30));
    CATCH_CHECK(last_event_received_.operation == Executor::OperationType::UNINSTALLING);
    CATCH_CHECK(last_event_received_.status == Executor::OperationStatus::SUCCESS);
}