#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "catch_amalgamated.hpp"
#include "network/http/HttpServer.h"
#include "util/IRequestDispatcher.h"

namespace cosmo::network::http {
namespace {

    using namespace std::chrono_literals;

    constexpr auto kShutdownOverlapDelay = 50ms;

    class BlockingDispatchState {
    public:
        void Enter(std::string interface, std::string mtk, std::string body) {
            std::unique_lock<std::mutex> lock(mutex_);
            interface_  = std::move(interface);
            mtk_        = std::move(mtk);
            body_       = std::move(body);
            is_entered_ = true;
            condition_.notify_all();
            condition_.wait(lock, [this]() { return is_released_; });
        }

        bool WaitUntilEntered() {
            std::unique_lock<std::mutex> lock(mutex_);
            return condition_.wait_for(lock, 2s, [this]() { return is_entered_; });
        }

        void Release() {
            std::lock_guard<std::mutex> lock(mutex_);
            is_released_ = true;
            condition_.notify_all();
        }

        std::string Interface() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return interface_;
        }

        std::string Mtk() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return mtk_;
        }

        std::string Body() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return body_;
        }

    private:
        mutable std::mutex mutex_;
        std::condition_variable condition_;
        bool is_entered_{false};
        bool is_released_{false};
        std::string interface_;
        std::string mtk_;
        std::string body_;
    };

    class BlockingDispatcher final : public cosmo::IRequestDispatcher {
    public:
        explicit BlockingDispatcher(std::shared_ptr<BlockingDispatchState> state)
            : state_(std::move(state)) {}

        bool SupportsRoute(const std::string& interface) override {
            return interface == "/test/request-lifetime";
        }

        bool DispatchRequest(const std::string& interface, const std::string& mtk, const std::string& body,
                             std::string& response) override {
            state_->Enter(interface, mtk, body);
            response = R"({"ok":true})";
            return true;
        }

    private:
        std::shared_ptr<BlockingDispatchState> state_;
    };

    class ReleaseGuard {
    public:
        explicit ReleaseGuard(std::shared_ptr<BlockingDispatchState> state) : state_(std::move(state)) {}

        ~ReleaseGuard() {
            state_->Release();
        }

        ReleaseGuard(const ReleaseGuard&)            = delete;
        ReleaseGuard& operator=(const ReleaseGuard&) = delete;

    private:
        std::shared_ptr<BlockingDispatchState> state_;
    };

    class HttpServerRunner {
    public:
        explicit HttpServerRunner(std::shared_ptr<BlockingDispatchState> state) : state_(std::move(state)) {}

        ~HttpServerRunner() {
            server_.UnInitialize();
            Join();
        }

        HttpServerRunner(const HttpServerRunner&)            = delete;
        HttpServerRunner& operator=(const HttpServerRunner&) = delete;

        bool Start(std::uint16_t port) {
            HttpServerCallbacks callbacks{
                []() { return std::string("/tmp/cosmo-http-lifetime-test"); },
                []() { return std::string("/tmp"); },
            };
            if (!server_.Initialize(
                    "127.0.0.1", port,
                    [state = state_]() { return std::make_unique<BlockingDispatcher>(state); },
                    std::move(callbacks))) {
                return false;
            }
            event_thread_ = std::thread([this]() { server_.DispatchMsg(); });
            return true;
        }

        HttpServer& Server() {
            return server_;
        }

        void Join() {
            if (event_thread_.joinable()) {
                event_thread_.join();
            }
        }

    private:
        std::shared_ptr<BlockingDispatchState> state_;
        HttpServer server_;
        std::thread event_thread_;
    };

    class ScopedFd {
    public:
        explicit ScopedFd(int fd = -1) : fd_(fd) {}

        ~ScopedFd() {
            Reset();
        }

        ScopedFd(const ScopedFd&)            = delete;
        ScopedFd& operator=(const ScopedFd&) = delete;

        ScopedFd(ScopedFd&& other) noexcept : fd_(other.fd_) {
            other.fd_ = -1;
        }

        ScopedFd& operator=(ScopedFd&& other) noexcept {
            if (this != &other) {
                Reset();
                fd_       = other.fd_;
                other.fd_ = -1;
            }
            return *this;
        }

        int Get() const {
            return fd_;
        }

        void Reset() {
            if (fd_ >= 0) {
                close(fd_);
                fd_ = -1;
            }
        }

    private:
        int fd_;
    };

    class ScopedSignalIgnore {
    public:
        using SignalHandler = void (*)(int);

        explicit ScopedSignalIgnore(int signal_number)
            : signal_number_(signal_number), previous_handler_(std::signal(signal_number, SIG_IGN)) {}

        ~ScopedSignalIgnore() {
            if (previous_handler_ != SIG_ERR) {
                std::signal(signal_number_, previous_handler_);
            }
        }

        ScopedSignalIgnore(const ScopedSignalIgnore&)            = delete;
        ScopedSignalIgnore& operator=(const ScopedSignalIgnore&) = delete;

    private:
        int signal_number_;
        SignalHandler previous_handler_;
    };

    std::uint16_t FindAvailablePort() {
        ScopedFd socket_fd(socket(AF_INET, SOCK_STREAM, 0));
        if (socket_fd.Get() < 0) {
            return 0;
        }

        sockaddr_in address{};
        address.sin_family      = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port        = 0;
        if (bind(socket_fd.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            return 0;
        }

        socklen_t address_length = sizeof(address);
        if (getsockname(socket_fd.Get(), reinterpret_cast<sockaddr*>(&address), &address_length) != 0) {
            return 0;
        }
        return ntohs(address.sin_port);
    }

    ScopedFd Connect(std::uint16_t port) {
        ScopedFd socket_fd(socket(AF_INET, SOCK_STREAM, 0));
        if (socket_fd.Get() < 0) {
            return ScopedFd{};
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port   = htons(port);
        if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
            connect(socket_fd.Get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            return ScopedFd{};
        }
        return socket_fd;
    }

    bool SendAll(int socket_fd, const std::string& data) {
        std::size_t sent_size{0};
        while (sent_size < data.size()) {
            auto result = send(socket_fd, data.data() + sent_size, data.size() - sent_size, 0);
            if (result <= 0) {
                return false;
            }
            sent_size += static_cast<std::size_t>(result);
        }
        return true;
    }

}  // namespace

TEST_CASE("HttpServer keeps libevent requests on the event thread during shutdown", "[http-server][thread]") {
    ScopedSignalIgnore ignore_sigpipe(SIGPIPE);
    auto state = std::make_shared<BlockingDispatchState>();
    HttpServerRunner runner(state);
    ReleaseGuard release_guard(state);

    auto port = FindAvailablePort();
    REQUIRE(port != 0);
    REQUIRE(runner.Start(port));

    auto client = Connect(port);
    REQUIRE(client.Get() >= 0);

    const std::string body = R"({"value":7})";
    const std::string request =
        "POST /test/request-lifetime HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: application/json\r\n"
        "mtk: lifetime-token\r\n"
        "Content-Length: " +
        std::to_string(body.size()) +
        "\r\n"
        "Connection: close\r\n\r\n" +
        body;
    REQUIRE(SendAll(client.Get(), request));
    REQUIRE(state->WaitUntilEntered());

    shutdown(client.Get(), SHUT_RDWR);
    client.Reset();

    std::thread stop_thread([&runner]() { runner.Server().UnInitialize(); });
    std::this_thread::sleep_for(kShutdownOverlapDelay);
    state->Release();
    stop_thread.join();
    runner.Join();

    CHECK(state->Interface() == "/test/request-lifetime");
    CHECK(state->Mtk() == "lifetime-token");
    CHECK(state->Body() == body);
}

}  // namespace cosmo::network::http
