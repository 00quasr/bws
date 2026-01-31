#include "infrastructure/network/AsioContext.hpp"

#include <spdlog/spdlog.h>

namespace netpulse::infra {

AsioContext::AsioContext(size_t threadCount)
    : threadCount_(threadCount > 0 ? threadCount : 1) {
    spdlog::info("AsioContext created with {} threads", threadCount_);
}

AsioContext::~AsioContext() {
    stop();
}

void AsioContext::start() {
    if (running_.exchange(true)) {
        return;
    }

    workGuard_.emplace(asio::make_work_guard(ioContext_));

    threads_.reserve(threadCount_);
    for (size_t i = 0; i < threadCount_; ++i) {
        threads_.emplace_back([this, i]() {
            spdlog::debug("Asio worker thread {} started", i);
            ioContext_.run();
            spdlog::debug("Asio worker thread {} stopped", i);
        });
    }

    spdlog::info("AsioContext started with {} worker threads", threadCount_);
}

void AsioContext::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    workGuard_.reset();
    ioContext_.stop();

    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();

    ioContext_.restart();
    spdlog::info("AsioContext stopped");
}

AsioContext& AsioContext::instance() {
    static AsioContext instance(4);
    return instance;
}

} // namespace netpulse::infra
