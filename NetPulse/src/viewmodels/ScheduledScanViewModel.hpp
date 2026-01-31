#pragma once

#include "core/types/ScheduledPortScan.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/database/ScheduledScanRepository.hpp"
#include "infrastructure/network/ScheduledPortScanner.hpp"

#include <QObject>
#include <memory>
#include <optional>
#include <vector>

namespace netpulse::viewmodels {

class ScheduledScanViewModel : public QObject {
    Q_OBJECT

public:
    explicit ScheduledScanViewModel(std::shared_ptr<infra::Database> db,
                                    std::shared_ptr<infra::ScheduledPortScanner> scheduler,
                                    QObject* parent = nullptr);

    int64_t addSchedule(const std::string& name, const std::string& targetAddress,
                        core::PortRange portRange = core::PortRange::Common,
                        int intervalMinutes = 60);
    int64_t addScheduleWithConfig(const core::ScheduledScanConfig& config);
    void updateSchedule(const core::ScheduledScanConfig& config);
    void removeSchedule(int64_t scheduleId);

    void setEnabled(int64_t scheduleId, bool enabled);
    void runNow(int64_t scheduleId);

    std::optional<core::ScheduledScanConfig> getSchedule(int64_t scheduleId) const;
    std::vector<core::ScheduledScanConfig> getAllSchedules() const;
    std::vector<core::ScheduledScanConfig> getEnabledSchedules() const;

    std::vector<core::PortScanDiff> getDiffs(int64_t scheduleId, int limit = 100) const;
    std::vector<core::PortScanDiff> getDiffsByAddress(const std::string& address,
                                                       int limit = 100) const;

    std::string exportDiffReport(const core::PortScanDiff& diff) const;
    std::string exportAllDiffsReport(int64_t scheduleId) const;

    void startScheduler();
    void stopScheduler();
    bool isSchedulerRunning() const;

signals:
    void scheduleAdded(int64_t scheduleId);
    void scheduleUpdated(int64_t scheduleId);
    void scheduleRemoved(int64_t scheduleId);
    void scanCompleted(int64_t scheduleId, int portsScanned, int openPorts);
    void diffDetected(int64_t scheduleId, int changesCount);
    void schedulerStarted();
    void schedulerStopped();

private:
    void onScanComplete(int64_t scheduleId, const std::vector<core::PortScanResult>& results);
    void onDiffDetected(int64_t scheduleId, const core::PortScanDiff& diff);
    void loadSchedulesFromDatabase();

    std::shared_ptr<infra::Database> db_;
    std::shared_ptr<infra::ScheduledPortScanner> scheduler_;
    std::unique_ptr<infra::ScheduledScanRepository> repo_;
};

} // namespace netpulse::viewmodels
