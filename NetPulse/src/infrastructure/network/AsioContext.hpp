#pragma once

#include <asio.hpp>
#include <atomic>
#include <memory>
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
    asio::io_context ioContext_;
    std::unique_ptr<asio::io_context::work> work_;
    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    size_t threadCount_;
};

} // namespace netpulse::infra
