#pragma once

#include "core/services/IPortScanner.hpp"
#include "infrastructure/network/AsioContext.hpp"

#include <asio.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <semaphore>

namespace netpulse::infra {

class PortScanner : public core::IPortScanner {
public:
    explicit PortScanner(AsioContext& context);
    ~PortScanner() override;

    void scanAsync(const core::PortScanConfig& config, ResultCallback onResult,
                   ProgressCallback onProgress, CompletionCallback onComplete) override;

    void cancel() override;
    bool isScanning() const override;

private:
    void scanPort(const std::string& address, uint16_t port, std::chrono::milliseconds timeout,
                  ResultCallback onResult);

    AsioContext& context_;
    std::atomic<bool> scanning_{false};
    std::atomic<bool> cancelled_{false};
    std::unique_ptr<std::counting_semaphore<>> semaphore_;
    mutable std::mutex mutex_;
};

} // namespace netpulse::infra
