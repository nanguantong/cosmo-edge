#include "catch_amalgamated.hpp"
/*
 * test_instance_pool.cc — InstancePool unit tests
 *
 * Tests the InstancePool object lifecycle and pooling behaviors.
 */
#include <memory>
#include <string>

#include "util/ErrorCode.h"
#include "util/InstancePool.h"

using namespace cosmo;

class MockTaskInstance {
public:
    MockTaskInstance(const std::string& atomic_code, const std::string& cfg_path,
                     const std::string& model_path)
        : atomic_code_(atomic_code), cfg_path_(cfg_path), model_path_(model_path) {}

    util::ErrorEnum Init() {
        if (atomic_code_ == "fail") {
            return util::ErrorEnum::Failed;
        }
        return util::ErrorEnum::Success;
    }

    std::string atomic_code_;
    std::string cfg_path_;
    std::string model_path_;
};

using MockTaskInstancePtr = std::shared_ptr<MockTaskInstance>;

TEST_CASE("InstancePool: Basic Lifecycle", "[instance-pool]") {
    InstancePool<MockTaskInstance, MockTaskInstancePtr> pool("TestPool", 2, 2);

    SECTION("GetInst without prior CreateTask creates an instance") {
        auto inst = pool.GetInst("code", "cfg", "model", 1000);
        REQUIRE(inst != nullptr);
        REQUIRE(inst->atomic_code_ == "code");

        // Return it
        pool.ReturnInst(inst);
    }

    SECTION("Init failure returns nullptr") {
        auto inst = pool.GetInst("fail", "cfg", "model", 100);
        REQUIRE(inst == nullptr);
    }

    SECTION("Pool respects max instance limit") {
        // We configure 2 max_inst_count and 2 inst_per_tasks
        // Let's create enough tasks to allow 2 instances
        pool.CreateTask("code1", "cfg", "model");
        pool.CreateTask("code2", "cfg", "model");
        pool.CreateTask("code3", "cfg", "model");
        pool.CreateTask("code4", "cfg", "model");

        auto inst1 = pool.GetInst("code1", "cfg", "model", 100);
        auto inst2 = pool.GetInst("code2", "cfg", "model", 100);
        REQUIRE(inst1 != nullptr);
        REQUIRE(inst2 != nullptr);
        REQUIRE(inst1 != inst2);

        // Third should fail due to max instance limit and no free instances
        auto inst3 = pool.GetInst("code3", "cfg", "model", 10);
        REQUIRE(inst3 == nullptr);

        pool.ReturnInst(inst1);
        pool.ReturnInst(inst2);
    }
}
