#pragma once

#include <asio.hpp>
#include <atomic>
#include <optional>
#include <thread>
#include <vector>

namespace netpulse::infra {

class AsioContext {
public:
    explicit AsioContext(size_t threadCount = std::thread::hardware_concurrency());
    ~AsioContext();

    AsioContext(const AsioContext&) = delete;
    AsioContext& operator=(const AsioContext&) = delete;

    void start();
    void stop();

    asio::io_context& getContext() { return ioContext_; }

    template <typename Handler>
    void post(Handler&& handler) {
        asio::post(ioContext_, std::forward<Handler>(handler));
    }

    static AsioContext& instance();

private:
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    asio::io_context ioContext_;
    std::optional<WorkGuard> workGuard_;
    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    size_t threadCount_;
};

} // namespace netpulse::infra
