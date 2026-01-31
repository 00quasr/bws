#include "viewmodels/ScheduledScanViewModel.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <sstream>

namespace netpulse::viewmodels {

namespace {

std::string timePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
    gmtime_r(&time, &tm);
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return buffer;
}

} // namespace

ScheduledScanViewModel::ScheduledScanViewModel(std::shared_ptr<infra::Database> db,
                                               std::shared_ptr<infra::ScheduledPortScanner> scheduler,
                                               QObject* parent)
    : QObject(parent),
      db_(std::move(db)),
      scheduler_(std::move(scheduler)),
      repo_(std::make_unique<infra::ScheduledScanRepository>(db_)) {

    scheduler_->setScanCompleteCallback(
        [this](int64_t scheduleId, const std::vector<core::PortScanResult>& results) {
            onScanComplete(scheduleId, results);
        });

    scheduler_->setDiffCallback([this](int64_t scheduleId, const core::PortScanDiff& diff) {
        onDiffDetected(scheduleId, diff);
    });

    loadSchedulesFromDatabase();
}

void ScheduledScanViewModel::loadSchedulesFromDatabase() {
    auto schedules = repo_->findAll();
    for (const auto& config : schedules) {
        scheduler_->addSchedule(config);
    }
    spdlog::info("Loaded {} scheduled scans from database", schedules.size());
}

int64_t ScheduledScanViewModel::addSchedule(const std::string& name, const std::string& targetAddress,
                                            core::PortRange portRange, int intervalMinutes) {
    core::ScheduledScanConfig config;
    config.name = name;
    config.targetAddress = targetAddress;
    config.portRange = portRange;
    config.intervalMinutes = intervalMinutes;
    config.enabled = true;
    config.notifyOnChanges = true;

    return addScheduleWithConfig(config);
}

int64_t ScheduledScanViewModel::addScheduleWithConfig(const core::ScheduledScanConfig& config) {
    auto id = repo_->insertSchedule(config);

    auto savedConfig = config;
    savedConfig.id = id;
    scheduler_->addSchedule(savedConfig);

    emit scheduleAdded(id);
    spdlog::info("Added scheduled scan: {} (ID: {})", config.name, id);

    return id;
}

void ScheduledScanViewModel::updateSchedule(const core::ScheduledScanConfig& config) {
    repo_->updateSchedule(config);
    scheduler_->updateSchedule(config);

    emit scheduleUpdated(config.id);
    spdlog::info("Updated scheduled scan: {} (ID: {})", config.name, config.id);
}

void ScheduledScanViewModel::removeSchedule(int64_t scheduleId) {
    scheduler_->removeSchedule(scheduleId);
    repo_->removeSchedule(scheduleId);

    emit scheduleRemoved(scheduleId);
    spdlog::info("Removed scheduled scan: {}", scheduleId);
}

void ScheduledScanViewModel::setEnabled(int64_t scheduleId, bool enabled) {
    repo_->setEnabled(scheduleId, enabled);
    scheduler_->enableSchedule(scheduleId, enabled);

    emit scheduleUpdated(scheduleId);
}

void ScheduledScanViewModel::runNow(int64_t scheduleId) {
    scheduler_->runNow(scheduleId);
}

std::optional<core::ScheduledScanConfig> ScheduledScanViewModel::getSchedule(
    int64_t scheduleId) const {
    return repo_->findById(scheduleId);
}

std::vector<core::ScheduledScanConfig> ScheduledScanViewModel::getAllSchedules() const {
    return repo_->findAll();
}

std::vector<core::ScheduledScanConfig> ScheduledScanViewModel::getEnabledSchedules() const {
    return repo_->findEnabled();
}

std::vector<core::PortScanDiff> ScheduledScanViewModel::getDiffs(int64_t scheduleId,
                                                                  int limit) const {
    return repo_->getDiffs(scheduleId, limit);
}

std::vector<core::PortScanDiff> ScheduledScanViewModel::getDiffsByAddress(const std::string& address,
                                                                           int limit) const {
    return repo_->getDiffsByAddress(address, limit);
}

std::string ScheduledScanViewModel::exportDiffReport(const core::PortScanDiff& diff) const {
    nlohmann::json j;
    j["target_address"] = diff.targetAddress;
    j["previous_scan_time"] = timePointToString(diff.previousScanTime);
    j["current_scan_time"] = timePointToString(diff.currentScanTime);
    j["total_ports_scanned"] = diff.totalPortsScanned;
    j["open_ports_before"] = diff.openPortsBefore;
    j["open_ports_after"] = diff.openPortsAfter;
    j["new_open_ports"] = diff.newOpenPorts();
    j["new_closed_ports"] = diff.newClosedPorts();

    j["changes"] = nlohmann::json::array();
    for (const auto& change : diff.changes) {
        nlohmann::json changeObj;
        changeObj["port"] = change.port;
        changeObj["change_type"] = change.changeTypeToString();
        changeObj["previous_state"] =
            core::PortScanResult::portStateToString(change.previousState);
        changeObj["current_state"] =
            core::PortScanResult::portStateToString(change.currentState);
        changeObj["service"] = change.serviceName;
        j["changes"].push_back(changeObj);
    }

    return j.dump(2);
}

std::string ScheduledScanViewModel::exportAllDiffsReport(int64_t scheduleId) const {
    auto schedule = repo_->findById(scheduleId);
    auto diffs = repo_->getDiffs(scheduleId);

    nlohmann::json j;
    if (schedule) {
        j["schedule_name"] = schedule->name;
        j["target_address"] = schedule->targetAddress;
        j["interval_minutes"] = schedule->intervalMinutes;
    }

    j["diffs"] = nlohmann::json::array();
    for (const auto& diff : diffs) {
        nlohmann::json diffObj;
        diffObj["previous_scan_time"] = timePointToString(diff.previousScanTime);
        diffObj["current_scan_time"] = timePointToString(diff.currentScanTime);
        diffObj["changes_count"] = diff.changes.size();
        diffObj["new_open_ports"] = diff.newOpenPorts();
        diffObj["new_closed_ports"] = diff.newClosedPorts();
        diffObj["open_ports_before"] = diff.openPortsBefore;
        diffObj["open_ports_after"] = diff.openPortsAfter;

        diffObj["changes"] = nlohmann::json::array();
        for (const auto& change : diff.changes) {
            nlohmann::json changeObj;
            changeObj["port"] = change.port;
            changeObj["type"] = change.changeTypeToString();
            changeObj["service"] = change.serviceName;
            diffObj["changes"].push_back(changeObj);
        }

        j["diffs"].push_back(diffObj);
    }

    return j.dump(2);
}

void ScheduledScanViewModel::startScheduler() {
    scheduler_->start();
    emit schedulerStarted();
}

void ScheduledScanViewModel::stopScheduler() {
    scheduler_->stop();
    emit schedulerStopped();
}

bool ScheduledScanViewModel::isSchedulerRunning() const {
    return scheduler_->isRunning();
}

void ScheduledScanViewModel::onScanComplete(int64_t scheduleId,
                                             const std::vector<core::PortScanResult>& results) {
    repo_->updateLastRunAt(scheduleId, std::chrono::system_clock::now());

    int openPorts = 0;
    for (const auto& result : results) {
        if (result.state == core::PortState::Open) {
            openPorts++;
        }
    }

    emit scanCompleted(scheduleId, static_cast<int>(results.size()), openPorts);
}

void ScheduledScanViewModel::onDiffDetected(int64_t scheduleId, const core::PortScanDiff& diff) {
    repo_->insertDiff(diff, scheduleId);
    emit diffDetected(scheduleId, static_cast<int>(diff.changes.size()));
}

} // namespace netpulse::viewmodels
