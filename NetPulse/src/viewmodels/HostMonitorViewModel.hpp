#pragma once

#include "core/types/Host.hpp"
#include "infrastructure/database/HostRepository.hpp"
#include "infrastructure/database/MetricsRepository.hpp"
#include "infrastructure/network/PingService.hpp"

#include <QObject>
#include <memory>
#include <optional>

namespace netpulse::viewmodels {

class HostMonitorViewModel : public QObject {
    Q_OBJECT

public:
    explicit HostMonitorViewModel(std::shared_ptr<infra::Database> db,
                                  std::shared_ptr<infra::PingService> pingService,
                                  QObject* parent = nullptr);

    // Host CRUD operations
    int64_t addHost(const std::string& name, const std::string& address);
    void updateHost(const core::Host& host);
    void removeHost(int64_t id);

    std::optional<core::Host> getHost(int64_t id) const;
    std::vector<core::Host> getAllHosts() const;

    // Monitoring control
    void startMonitoringHost(int64_t hostId);
    void stopMonitoringHost(int64_t hostId);
    void enableHost(int64_t hostId, bool enabled);

    // Data operations
    std::string exportHostData(int64_t hostId, const std::string& format) const;
    void clearHostHistory(int64_t hostId);

signals:
    void hostAdded(int64_t hostId);
    void hostUpdated(int64_t hostId);
    void hostRemoved(int64_t hostId);
    void monitoringStarted(int64_t hostId);
    void monitoringStopped(int64_t hostId);

private:
    std::shared_ptr<infra::Database> db_;
    std::shared_ptr<infra::PingService> pingService_;
    std::unique_ptr<infra::HostRepository> hostRepo_;
    std::unique_ptr<infra::MetricsRepository> metricsRepo_;
};

} // namespace netpulse::viewmodels
