#include "catch_amalgamated.hpp"
/*
 * test_mqtt_lifecycle_service_impl.cc — MqttLifecycleServiceImpl unit tests
 *
 * Strategy: Test construction/destruction and state queries.
 * Actual MQTT connection requires a broker, so connection tests are minimal.
 */
#include <arpa/inet.h>
#include <event2/dns.h>
#include <event2/event.h>
#include <event2/util.h>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "mock/MockConfigNetworkService.h"
#include "mock/MockConfigReadService.h"
#include "mock/MockDeviceInfoService.h"
#include "mock/MockServiceRegistry.h"
#include "service/network/impl/MqttLifecycleServiceImpl.h"
#include "util/IRequestDispatcher.h"

using namespace cosmo::service;

namespace {

class StubDispatcher : public cosmo::IRequestDispatcher {
public:
    bool SupportsRoute(const std::string& /*uri*/) override {
        return false;
    }
    cosmo::RequestAdmission InspectRequest(cosmo::RequestDispatchContext& /*context*/,
                                           bool /*require_known_route*/) override {
        return cosmo::RequestAdmission::kRouteNotFound;
    }
    bool DispatchRequest(const cosmo::RequestDispatchContext& /*context*/, const std::string& /*body*/,
                         std::string& /*response*/) override {
        return false;
    }
};

struct EventBaseDeleter {
    void operator()(event_base* base) const {
        if (base != nullptr) {
            event_base_free(base);
        }
    }
};

struct DnsBaseDeleter {
    void operator()(evdns_base* base) const {
        if (base != nullptr) {
            evdns_base_free(base, 0);
        }
    }
};

struct DnsTestResult {
    ~DnsTestResult() {
        if (addresses != nullptr) {
            evutil_freeaddrinfo(addresses);
        }
    }

    bool completed{false};
    int error{EVUTIL_EAI_FAIL};
    evutil_addrinfo* addresses{nullptr};
};

}  // namespace

TEST_CASE("MqttLifecycleServiceImpl: cancelled DNS callback is drained before teardown",
          "[mqtt-lifecycle][dns][lifecycle]") {
    std::unique_ptr<event_base, EventBaseDeleter> event_base_ptr(event_base_new());
    REQUIRE(event_base_ptr != nullptr);
    std::unique_ptr<evdns_base, DnsBaseDeleter> dns_base_ptr(evdns_base_new(event_base_ptr.get(), 0));
    REQUIRE(dns_base_ptr != nullptr);
    REQUIRE(evdns_base_nameserver_ip_add(dns_base_ptr.get(), "127.0.0.1:65535") == 0);

    evutil_addrinfo hints{};
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    DnsTestResult result;
    const auto callback = [](int error, evutil_addrinfo* addresses, void* context) {
        auto* resolution      = static_cast<DnsTestResult*>(context);
        resolution->completed = true;
        resolution->error     = error;
        resolution->addresses = addresses;
    };
    auto* request = evdns_getaddrinfo(dns_base_ptr.get(), "pending-dns-request.example", nullptr, &hints,
                                      callback, &result);
    REQUIRE(request != nullptr);

    CHECK(cosmo::service::detail::CancelAndDrainDnsRequest(event_base_ptr.get(), request, &result.completed));
    CHECK(result.completed);
    CHECK(result.error == EVUTIL_EAI_CANCEL);
    CHECK(result.addresses == nullptr);
}

TEST_CASE("MqttLifecycleServiceImpl: construction and destruction", "[mqtt-lifecycle]") {
    REQUIRE_NOTHROW(
        []() { MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); }); }());
}

TEST_CASE("MqttLifecycleServiceImpl: initial state", "[mqtt-lifecycle]") {
    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });

    SECTION("IsMqttRegistered returns false initially") {
        REQUIRE(sut.IsMqttRegistered() == false);
    }

    SECTION("IsMqttEnabled returns false initially") {
        REQUIRE(sut.IsMqttEnabled() == false);
    }
}

TEST_CASE("MqttLifecycleServiceImpl: MqttStop without MqttStart is safe", "[mqtt-lifecycle]") {
    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });
    REQUIRE_NOTHROW(sut.MqttStop());
}

TEST_CASE("MqttLifecycleServiceImpl: double MqttStop is safe", "[mqtt-lifecycle]") {
    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });
    REQUIRE_NOTHROW(sut.MqttStop());
    REQUIRE_NOTHROW(sut.MqttStop());
}

TEST_CASE("MqttLifecycleServiceImpl: permanent shutdown is idempotent and not restartable",
          "[mqtt-lifecycle][lifecycle]") {
    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });
    REQUIRE_NOTHROW(sut.MqttShutdown());
    REQUIRE_NOTHROW(sut.MqttShutdown());

    // Restart must return before consulting ServiceRegistry configuration.
    REQUIRE_NOTHROW(sut.MqttStart());
    REQUIRE_NOTHROW(sut.MqttStop());
    REQUIRE_FALSE(sut.IsMqttEnabled());
    REQUIRE_FALSE(sut.IsMqttRegistered());
}

TEST_CASE("MqttLifecycleServiceImpl: concurrent permanent shutdown is serialized",
          "[mqtt-lifecycle][lifecycle][concurrency]") {
    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&sut]() { sut.MqttShutdown(); });
    }
    for (auto& worker : workers) {
        worker.join();
    }
    REQUIRE_FALSE(sut.IsMqttEnabled());
    REQUIRE_FALSE(sut.IsMqttRegistered());
}

TEST_CASE("MqttLifecycleServiceImpl: MqttStart and MqttStop lifecycle", "[mqtt-lifecycle]") {
    cosmo::test::MockServiceRegistry mocks;

    ALLOW_CALL(mocks.configReadSvc, GetRunMode()).RETURN(cosmo::RunMode::RunModeStandAlone);
    cosmo::service::MqttParam mqttP;
    mqttP.enable = false;  // Disabled — no actual connection
    ALLOW_CALL(mocks.configNetSvc, GetMqttParam()).RETURN(mqttP);
    ALLOW_CALL(mocks.deviceInfoSvc, GetDevSn()).RETURN("SN-TEST");

    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });
    REQUIRE_NOTHROW(sut.MqttStart());
    REQUIRE_NOTHROW(sut.MqttStop());
}

TEST_CASE("MqttLifecycleServiceImpl: concurrent disabled start and stop is serialized",
          "[mqtt-lifecycle][concurrency]") {
    cosmo::test::MockServiceRegistry mocks;

    ALLOW_CALL(mocks.configReadSvc, GetRunMode()).RETURN(cosmo::RunMode::RunModeStandAlone);
    cosmo::service::MqttParam mqtt_param;
    mqtt_param.enable = false;
    ALLOW_CALL(mocks.configNetSvc, GetMqttParam()).RETURN(mqtt_param);

    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&sut, i]() {
            for (int attempt = 0; attempt < 10; ++attempt) {
                if ((attempt + i) % 2 == 0) {
                    sut.MqttStart();
                } else {
                    sut.MqttStop();
                }
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    sut.MqttStop();
    REQUIRE_FALSE(sut.IsMqttEnabled());
    REQUIRE_FALSE(sut.IsMqttRegistered());
}

TEST_CASE("MqttLifecycleServiceImpl: Stop interrupts DNS resolution promptly",
          "[mqtt-lifecycle][dns][lifecycle]") {
    cosmo::test::MockServiceRegistry mocks;

    ALLOW_CALL(mocks.configReadSvc, GetRunMode()).RETURN(cosmo::RunMode::RunModeStandAlone);
    cosmo::service::MqttParam mqtt_param;
    mqtt_param.enable = true;
    mqtt_param.url    = "mqtt-stop-test.invalid";
    mqtt_param.port   = 1883;
    ALLOW_CALL(mocks.configNetSvc, GetMqttParam()).RETURN(mqtt_param);
    ALLOW_CALL(mocks.deviceInfoSvc, GetDevSn()).RETURN("SN-DNS-STOP-TEST");

    MqttLifecycleServiceImpl sut([]() { return std::make_unique<StubDispatcher>(); });
    sut.MqttStart();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto stop_started = std::chrono::steady_clock::now();
    sut.MqttStop();
    const auto stop_elapsed = std::chrono::steady_clock::now() - stop_started;

    CHECK(stop_elapsed < std::chrono::seconds(1));
    CHECK_FALSE(sut.IsMqttEnabled());
    CHECK_FALSE(sut.IsMqttRegistered());
}
