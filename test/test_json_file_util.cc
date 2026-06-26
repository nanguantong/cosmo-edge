#include "catch_amalgamated.hpp"
/*
 * test_json_file_util.cc — JsonFileUtil unit tests
 *
 * Tests JSON file read/write, array operations, and item find/update/delete.
 */
#include <cstdlib>

#include "nlohmann/json.hpp"
#include "util/FileUtil.h"
#include "util/JsonFileUtil.h"

using namespace cosmo::util;
using json = nlohmann::json;

namespace {
std::string CreateTestDir() {
    std::string base = "/tmp/cosmo_test_json_XXXXXX";
    char* dir        = mkdtemp(base.data());
    return dir ? std::string(dir) : "";
}
}  // namespace

TEST_CASE("JsonFileUtil: WriteJsonFile and ReadJsonFile", "[json-file-util]") {
    auto dir = CreateTestDir();
    REQUIRE_FALSE(dir.empty());

    std::string path = dir + "/test.json";

    SECTION("Write and read back") {
        json doc = {{"name", "cosmo"}, {"version", 1}};
        REQUIRE(JsonFileUtil::WriteJsonFile(path, doc) == ErrorEnum::Success);

        json loaded;
        REQUIRE(JsonFileUtil::ReadJsonFile(path, loaded) == ErrorEnum::Success);
        REQUIRE(loaded["name"] == "cosmo");
        REQUIRE(loaded["version"] == 1);
    }

    SECTION("Read non-existent file") {
        json loaded;
        auto err = JsonFileUtil::ReadJsonFile(dir + "/no.json", loaded);
        REQUIRE(err != ErrorEnum::Success);
    }

    RemovePath(dir);
}

TEST_CASE("JsonFileUtil: ReadJsonArray", "[json-file-util]") {
    auto dir = CreateTestDir();
    REQUIRE_FALSE(dir.empty());

    std::string path = dir + "/array.json";

    SECTION("Read valid array") {
        WriteFile(path, R"([{"id":"1"},{"id":"2"}])");
        json arr;
        REQUIRE(JsonFileUtil::ReadJsonArray(path, arr) == ErrorEnum::Success);
        REQUIRE(arr.is_array());
        REQUIRE(arr.size() == 2);
    }

    SECTION("Read non-existent file") {
        json arr;
        auto err = JsonFileUtil::ReadJsonArray(dir + "/no.json", arr);
        REQUIRE(err == ErrorEnum::Success);
        REQUIRE(arr.is_array());
        REQUIRE(arr.empty());
    }

    RemovePath(dir);
}

TEST_CASE("JsonFileUtil: AppendToJsonArray", "[json-file-util]") {
    auto dir = CreateTestDir();
    REQUIRE_FALSE(dir.empty());

    std::string path = dir + "/append.json";
    WriteFile(path, R"([{"id":"1"}])");

    json item = {{"id", "2"}, {"name", "new"}};
    REQUIRE(JsonFileUtil::AppendToJsonArray(path, item) == ErrorEnum::Success);

    json arr;
    REQUIRE(JsonFileUtil::ReadJsonArray(path, arr) == ErrorEnum::Success);
    REQUIRE(arr.size() == 2);
    REQUIRE(arr[1]["id"] == "2");

    RemovePath(dir);
}

TEST_CASE("JsonFileUtil: UpdateJsonArrayItem", "[json-file-util]") {
    auto dir = CreateTestDir();
    REQUIRE_FALSE(dir.empty());

    std::string path = dir + "/update.json";
    WriteFile(path, R"([{"id":"1","name":"old"},{"id":"2","name":"keep"}])");

    json new_item = {{"id", "1"}, {"name", "updated"}};
    REQUIRE(JsonFileUtil::UpdateJsonArrayItem(path, "id", "1", new_item) == ErrorEnum::Success);

    json arr;
    REQUIRE(JsonFileUtil::ReadJsonArray(path, arr) == ErrorEnum::Success);
    REQUIRE(arr[0]["name"] == "updated");
    REQUIRE(arr[1]["name"] == "keep");

    RemovePath(dir);
}

TEST_CASE("JsonFileUtil: DeleteJsonArrayItem", "[json-file-util]") {
    auto dir = CreateTestDir();
    REQUIRE_FALSE(dir.empty());

    std::string path = dir + "/delete.json";
    WriteFile(path, R"([{"id":"1"},{"id":"2"},{"id":"3"}])");

    REQUIRE(JsonFileUtil::DeleteJsonArrayItem(path, "id", "2") == ErrorEnum::Success);

    json arr;
    REQUIRE(JsonFileUtil::ReadJsonArray(path, arr) == ErrorEnum::Success);
    REQUIRE(arr.size() == 2);

    RemovePath(dir);
}

TEST_CASE("JsonFileUtil: FindItemInArray", "[json-file-util]") {
    json arr = json::parse(R"([{"id":"1","name":"a"},{"id":"2","name":"b"}])");

    SECTION("Found") {
        auto* item = JsonFileUtil::FindItemInArray(arr, "id", "2");
        REQUIRE(item != nullptr);
        REQUIRE((*item)["name"] == "b");
    }

    SECTION("Not found") {
        auto* item = JsonFileUtil::FindItemInArray(arr, "id", "99");
        REQUIRE(item == nullptr);
    }
}
