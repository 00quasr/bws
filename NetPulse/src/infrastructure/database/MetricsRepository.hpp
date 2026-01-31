#pragma once

#include "core/types/Alert.hpp"
#include "core/types/PingResult.hpp"
#include "core/types/PortScanResult.hpp"
#include "infrastructure/database/Database.hpp"

#include <chrono>
#include <memory>
#include <vector>

namespace netpulse::infra {

class MetricsRepository {
public:
    explicit MetricsRepository(std::shared_ptr<Database> db);

    // Ping results
    int64_t insertPingResult(const core::PingResult& result);
    std::vector<core::PingResult> getPingResults(int64_t hostId, int limit = 100);
    std::vector<core::PingResult> getPingResultsSince(
        int64_t hostId, std::chrono::system_clock::time_point since);
    core::PingStatistics getStatistics(int64_t hostId, int sampleCount = 100);
    void cleanupOldPingResults(std::chrono::hours maxAge);

    // Alerts
    int64_t insertAlert(const core::Alert& alert);
    std::vector<core::Alert> getAlerts(int limit = 100);
    std::vector<core::Alert> getUnacknowledgedAlerts();
    void acknowledgeAlert(int64_t id);
    void acknowledgeAll();
    void cleanupOldAlerts(std::chrono::hours maxAge);

    // Port scan results
    int64_t insertPortScanResult(const core::PortScanResult& result);
    void insertPortScanResults(const std::vector<core::PortScanResult>& results);
    std::vector<core::PortScanResult> getPortScanResults(const std::string& address,
                                                          int limit = 1000);
    void cleanupOldPortScans(std::chrono::hours maxAge);

    // Export
    std::string exportToJson(int64_t hostId);
    std::string exportToCsv(int64_t hostId);

private:
    std::shared_ptr<Database> db_;
};

} // namespace netpulse::infra
