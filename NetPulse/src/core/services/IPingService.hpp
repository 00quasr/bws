#pragma once

#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"

#include <functional>
#include <future>
#include <memory>

namespace netpulse::core {

class IPingService {
public:
    using PingCallback = std::function<void(const PingResult&)>;

    virtual ~IPingService() = default;

    virtual std::future<PingResult> pingAsync(const std::string& address,
                                              std::chrono::milliseconds timeout) = 0;

    virtual void startMonitoring(const Host& host, PingCallback callback) = 0;
    virtual void stopMonitoring(int64_t hostId) = 0;
    virtual void stopAllMonitoring() = 0;

    virtual bool isMonitoring(int64_t hostId) const = 0;
};

} // namespace netpulse::core
