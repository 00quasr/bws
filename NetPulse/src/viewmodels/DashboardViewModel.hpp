/**
 * @file DashboardViewModel.hpp
 * @brief ViewModel for the main dashboard display.
 *
 * This file defines the DashboardViewModel class which provides data binding
 * and business logic for the main monitoring dashboard in the MVVM architecture.
 */

#pragma once

#include "core/types/Host.hpp"
#include "core/types/NetworkInterface.hpp"
#include "core/types/PingResult.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "infrastructure/network/PingService.hpp"

#include <QObject>
#include <memory>
#include <vector>

namespace netpulse::viewmodels {

/**
 * @brief ViewModel for the main monitoring dashboard.
 *
 * Provides data and operations for displaying host monitoring status,
 * ping results, statistics, and network interface information. Manages
 * the monitoring lifecycle and emits signals for UI updates.
 */
class DashboardViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a DashboardViewModel.
     * @param db Shared pointer to the database connection.
     * @param pingService Shared pointer to the ping service for monitoring.
     * @param parent Optional parent QObject for Qt ownership.
     */
    explicit DashboardViewModel(std::shared_ptr<infra::Database> db,
                                std::shared_ptr<infra::PingService> pingService,
                                QObject* parent = nullptr);

    /**
     * @brief Destroys the DashboardViewModel.
     */
    ~DashboardViewModel() override;

    /**
     * @brief Starts monitoring all enabled hosts.
     */
    void startMonitoring();

    /**
     * @brief Stops monitoring all hosts.
     */
    void stopMonitoring();

    /**
     * @brief Gets all configured hosts.
     * @return Vector of all hosts in the database.
     */
    std::vector<core::Host> getHosts() const;

    /**
     * @brief Gets recent ping results for a specific host.
     * @param hostId ID of the host to query.
     * @param limit Maximum number of results to return (default: 100).
     * @return Vector of recent ping results, ordered by timestamp descending.
     */
    std::vector<core::PingResult> getRecentResults(int64_t hostId, int limit = 100) const;

    /**
     * @brief Gets ping statistics for a specific host.
     * @param hostId ID of the host to query.
     * @return Aggregated statistics including latency, packet loss, and uptime.
     */
    core::PingStatistics getStatistics(int64_t hostId) const;

    /**
     * @brief Gets all available network interfaces.
     * @return Vector of network interfaces with their current statistics.
     */
    std::vector<core::NetworkInterface> getNetworkInterfaces() const;

    /**
     * @brief Gets the total number of configured hosts.
     * @return Total host count.
     */
    int hostCount() const;

    /**
     * @brief Gets the number of hosts currently responding.
     * @return Count of hosts with status Up.
     */
    int hostsUp() const;

    /**
     * @brief Gets the number of hosts not responding.
     * @return Count of hosts with status Down.
     */
    int hostsDown() const;

signals:
    /**
     * @brief Emitted when a new ping result is received.
     * @param hostId ID of the host that was pinged.
     * @param result The ping result data.
     */
    void pingResultReceived(int64_t hostId, const core::PingResult& result);

    /**
     * @brief Emitted when a host's status changes.
     * @param hostId ID of the host whose status changed.
     * @param status The new status of the host.
     */
    void hostStatusChanged(int64_t hostId, core::HostStatus status);

    /**
     * @brief Emitted when network interface statistics are updated.
     */
    void interfaceStatsUpdated();

private slots:
    void onPingResult(int64_t hostId, const core::PingResult& result);

private:
    void updateHostStatus(int64_t hostId, const core::PingResult& result);

    std::shared_ptr<infra::Database> db_;
    std::shared_ptr<infra::PingService> pingService_;
    std::unique_ptr<infra::HostRepository> hostRepo_;
    std::unique_ptr<infra::MetricsRepository> metricsRepo_;

    std::map<int64_t, int> consecutiveFailures_;
    int consecutiveFailuresThreshold_{3};
};

} // namespace netpulse::viewmodels
