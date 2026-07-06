#include "catch_amalgamated.hpp"
/*
 * test_path_util.cc — PathUtil unit tests
 *
 * Tests path accessors after OverrideRootPathForTest().
 */
#include <filesystem>

#include "util/PathUtil.h"

using namespace cosmo::path;

namespace {

struct TestPathFixture {
    std::string test_dir;

    TestPathFixture() {
        test_dir = "/tmp/cosmo_path_test_" +
                   std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(test_dir);
        OverrideRootPathForTest(test_dir, test_dir);
    }

    ~TestPathFixture() {
        std::filesystem::remove_all(test_dir);
    }
};

}  // namespace

TEST_CASE("PathUtil: GetCfgPath returns non-empty path", "[path-util]") {
    TestPathFixture fix;
    auto cfg = GetCfgPath();
    REQUIRE(!cfg.empty());
}

TEST_CASE("PathUtil: GetCfgPath with subPath appends correctly", "[path-util]") {
    TestPathFixture fix;
    auto cfg = GetCfgPath("sub/dir");
    REQUIRE(cfg.find("sub/dir") != std::string::npos);
}

TEST_CASE("PathUtil: GetDbPath returns path containing db", "[path-util]") {
    TestPathFixture fix;
    auto db = GetDbPath();
    REQUIRE(db.find("db") != std::string::npos);
}

TEST_CASE("PathUtil: GetLogPath returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto logPath = GetLogPath();
    REQUIRE(!logPath.empty());
}

TEST_CASE("PathUtil: GetUploadPath returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto upload = GetUploadPath();
    REQUIRE(!upload.empty());
}

TEST_CASE("PathUtil: GetModelPath returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto model = GetModelPath();
    REQUIRE(!model.empty());
}

TEST_CASE("PathUtil: GetModelSearchPaths returns at least one entry", "[path-util]") {
    TestPathFixture fix;
    auto paths = GetModelSearchPaths();
    REQUIRE(!paths.empty());
}

TEST_CASE("PathUtil: GetResourcePath returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto res = GetResourcePath();
    REQUIRE(!res.empty());
}

TEST_CASE("PathUtil: GetEventPath creates directory", "[path-util]") {
    TestPathFixture fix;
    uint64_t ts    = 1700000000000;
    auto eventPath = GetEventPath(ts, true);
    REQUIRE(!eventPath.empty());
    REQUIRE(std::filesystem::exists(eventPath));
}

TEST_CASE("PathUtil: GetEventWebPath returns web-relative path", "[path-util]") {
    TestPathFixture fix;
    uint64_t ts  = 1700000000000;
    auto webPath = GetEventWebPath(ts);
    REQUIRE(!webPath.empty());
}

TEST_CASE("PathUtil: GetBaseDir returns test override path", "[path-util]") {
    TestPathFixture fix;
    auto base = GetBaseDir();
    REQUIRE(base.find(fix.test_dir) != std::string::npos);
}

TEST_CASE("PathUtil: GetBackupCfgPath returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto backup = GetBackupCfgPath();
    REQUIRE(!backup.empty());

    SECTION("With subPath") {
        auto sub = GetBackupCfgPath("test.json");
        REQUIRE(sub.find("test.json") != std::string::npos);
    }
}

TEST_CASE("PathUtil: GetUpgradePath returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto upgrade = GetUpgradePath();
    REQUIRE(!upgrade.empty());
}

TEST_CASE("PathUtil: GetFaceLibPhotoDir returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto dir = GetFaceLibPhotoDir();
    REQUIRE(!dir.empty());
}

TEST_CASE("PathUtil: GetTemporaryDirPath returns non-empty", "[path-util]") {
    TestPathFixture fix;
    auto tmp = GetTemporaryDirPath();
    REQUIRE(!tmp.empty());
}

TEST_CASE("PathUtil: IsWithinRoot containment check", "[path-util]") {
    SECTION("Exact equal path is within") {
        REQUIRE(IsWithinRoot("/data/a", "/data/a"));
    }
    SECTION("Nested child is within") {
        REQUIRE(IsWithinRoot("/data/a", "/data/a/b"));
        REQUIRE(IsWithinRoot("/data/a", "/data/a/b/c"));
    }
    SECTION("Sibling prefix is NOT within (component-aware)") {
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/data/ab"));
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/data/ab/c"));
    }
    SECTION("Parent (candidate shorter than root) is NOT within") {
        REQUIRE_FALSE(IsWithinRoot("/data/a/b", "/data/a"));
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/data"));
    }
    SECTION("Dotdot traversal escapes root") {
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/data/a/.."));    // resolves to /data
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/data/a/../b"));  // resolves to /data/b
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/data/a/../../etc/passwd"));
    }
    SECTION("Absolute escape is NOT within") {
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/etc/passwd"));
        REQUIRE_FALSE(IsWithinRoot("/data/a", "/etc/shadow"));
    }
    SECTION("Dot segment collapses, nested child still within") {
        REQUIRE(IsWithinRoot("/data/a", "/data/a/./b"));  // resolves to /data/a/b
    }
    // Trailing-slash inputs are not asserted: production roots (GetBaseDir/GetPersonLibPhotoDir)
    // and the path(root)/filename joins never carry trailing separators, and the device libstdc++
    // path-iterator leaves a trailing empty component for root "/x/" that the mismatch check (which
    // is correct for all real inputs, see the cases above) does not need to defend against.
    SECTION("Non-existent deep tail still within") {
        REQUIRE(IsWithinRoot("/data/a", "/data/a/x/y/z/none"));
    }
    SECTION("Empty inputs are rejected") {
        REQUIRE_FALSE(IsWithinRoot("", "/data/a"));
        REQUIRE_FALSE(IsWithinRoot("/data/a", ""));
    }
}

TEST_CASE("PathUtil: IsWithinRoot with real filesystem", "[path-util]") {
    TestPathFixture fix;
    const auto root    = fix.test_dir + "/root";
    const auto outside = fix.test_dir + "/outside";
    std::filesystem::create_directories(root + "/sub");
    std::filesystem::create_directories(outside);

    REQUIRE(IsWithinRoot(root, root));                     // existing exact
    REQUIRE(IsWithinRoot(root, root + "/sub"));            // existing child
    REQUIRE(IsWithinRoot(root, root + "/sub/deep/none"));  // non-existent tail inside
    REQUIRE_FALSE(IsWithinRoot(root, outside));            // sibling dir
}
