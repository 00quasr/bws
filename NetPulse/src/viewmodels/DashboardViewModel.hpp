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

class DashboardViewModel : public QObject {
    Q_OBJECT

public:
    explicit DashboardViewModel(std::shared_ptr<infra::Database> db,
                                std::shared_ptr<infra::PingService> pingService,
                                QObject* parent = nullptr);
    ~DashboardViewModel() override;

    void startMonitoring();
    void stopMonitoring();

    std::vector<core::Host> getHosts() const;
    std::vector<core::PingResult> getRecentResults(int64_t hostId, int limit = 100) const;
    core::PingStatistics getStatistics(int64_t hostId) const;
    std::vector<core::NetworkInterface> getNetworkInterfaces() const;

    int hostCount() const;
    int hostsUp() const;
    int hostsDown() const;

signals:
    void pingResultReceived(int64_t hostId, const core::PingResult& result);
    void hostStatusChanged(int64_t hostId, core::HostStatus status);
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
