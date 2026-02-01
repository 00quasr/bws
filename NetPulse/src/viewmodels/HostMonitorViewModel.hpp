/**
 * @file HostMonitorViewModel.hpp
 * @brief ViewModel for individual host management and monitoring.
 *
 * This file defines the HostMonitorViewModel class which provides CRUD
 * operations for hosts and per-host monitoring control in the MVVM architecture.
 */

#pragma once

#include "core/types/Host.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "infrastructure/network/PingService.hpp"

#include <QObject>
#include <memory>
#include <optional>

namespace netpulse::viewmodels {

/**
 * @brief ViewModel for managing individual hosts.
 *
 * Provides CRUD operations for host entities, per-host monitoring control,
 * and data export functionality. Emits signals for UI synchronization when
 * host data changes.
 */
class HostMonitorViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs a HostMonitorViewModel.
     * @param db Shared pointer to the database connection.
     * @param pingService Shared pointer to the ping service for monitoring.
     * @param parent Optional parent QObject for Qt ownership.
     */
    explicit HostMonitorViewModel(std::shared_ptr<infra::Database> db,
                                  std::shared_ptr<infra::PingService> pingService,
                                  QObject* parent = nullptr);

    /**
     * @brief Adds a new host to the monitoring system.
     * @param name Display name for the host.
     * @param address IP address or hostname to monitor.
     * @return The ID of the newly created host.
     */
    int64_t addHost(const std::string& name, const std::string& address);

    /**
     * @brief Updates an existing host's configuration.
     * @param host The host object with updated fields.
     */
    void updateHost(const core::Host& host);

    /**
     * @brief Removes a host from the system.
     * @param id ID of the host to remove.
     */
    void removeHost(int64_t id);

    /**
     * @brief Gets a specific host by ID.
     * @param id ID of the host to retrieve.
     * @return The host if found, std::nullopt otherwise.
     */
    std::optional<core::Host> getHost(int64_t id) const;

    /**
     * @brief Gets all configured hosts.
     * @return Vector of all hosts in the database.
     */
    std::vector<core::Host> getAllHosts() const;

    /**
     * @brief Starts monitoring a specific host.
     * @param hostId ID of the host to start monitoring.
     */
    void startMonitoringHost(int64_t hostId);

    /**
     * @brief Stops monitoring a specific host.
     * @param hostId ID of the host to stop monitoring.
     */
    void stopMonitoringHost(int64_t hostId);

    /**
     * @brief Enables or disables a host for monitoring.
     * @param hostId ID of the host to modify.
     * @param enabled True to enable, false to disable.
     */
    void enableHost(int64_t hostId, bool enabled);

    /**
     * @brief Exports monitoring data for a host.
     * @param hostId ID of the host to export data for.
     * @param format Export format (e.g., "json", "csv").
     * @return The exported data as a string.
     */
    std::string exportHostData(int64_t hostId, const std::string& format) const;

    /**
     * @brief Clears all historical data for a host.
     * @param hostId ID of the host whose history to clear.
     */
    void clearHostHistory(int64_t hostId);

signals:
    /**
     * @brief Emitted when a new host is added.
     * @param hostId ID of the newly added host.
     */
    void hostAdded(int64_t hostId);

    /**
     * @brief Emitted when a host is updated.
     * @param hostId ID of the updated host.
     */
    void hostUpdated(int64_t hostId);

    /**
     * @brief Emitted when a host is removed.
     * @param hostId ID of the removed host.
     */
    void hostRemoved(int64_t hostId);

    /**
     * @brief Emitted when monitoring starts for a host.
     * @param hostId ID of the host being monitored.
     */
    void monitoringStarted(int64_t hostId);

    /**
     * @brief Emitted when monitoring stops for a host.
     * @param hostId ID of the host no longer being monitored.
     */
    void monitoringStopped(int64_t hostId);

private:
    std::shared_ptr<infra::Database> db_;
    std::shared_ptr<infra::PingService> pingService_;
    std::unique_ptr<infra::HostRepository> hostRepo_;
    std::unique_ptr<infra::MetricsRepository> metricsRepo_;
};

} // namespace netpulse::viewmodels
