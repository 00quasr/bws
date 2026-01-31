#pragma once

#include "core/types/Alert.hpp"
#include "core/types/PingResult.hpp"

#include <functional>
#include <vector>

namespace netpulse::core {

class IAlertService {
public:
    using AlertCallback = std::function<void(const Alert&)>;

    virtual ~IAlertService() = default;

    virtual void setThresholds(const AlertThresholds& thresholds) = 0;
    virtual AlertThresholds getThresholds() const = 0;

    virtual void processResult(int64_t hostId, const std::string& hostName,
                               const PingResult& result) = 0;

    virtual void subscribe(AlertCallback callback) = 0;
    virtual void unsubscribeAll() = 0;

    virtual std::vector<Alert> getRecentAlerts(int limit = 100) const = 0;
    virtual void acknowledgeAlert(int64_t alertId) = 0;
    virtual void clearAlerts() = 0;
};

} // namespace netpulse::core
