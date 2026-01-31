#include "infrastructure/network/ScheduledPortScanner.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <unordered_map>

namespace netpulse::infra {

ScheduledPortScanner::ScheduledPortScanner(AsioContext& context, core::IPortScanner& portScanner)
    : context_(context), portScanner_(portScanner) {
    spdlog::debug("ScheduledPortScanner initialized");
}

ScheduledPortScanner::~ScheduledPortScanner() {
    stop();
}

int64_t ScheduledPortScanner::generateId() {
    return nextId_++;
}

void ScheduledPortScanner::addSchedule(const core::ScheduledScanConfig& config) {
    std::lock_guard lock(mutex_);

    auto item = std::make_shared<ScheduledItem>();
    item->config = config;
    if (item->config.id == 0) {
        item->config.id = generateId();
    }
    item->config.createdAt = std::chrono::system_clock::now();
    item->timer = std::make_shared<asio::steady_timer>(context_.getContext());
    item->active = config.enabled && running_;

    schedules_[item->config.id] = item;

    spdlog::info("Added scheduled scan: {} (ID: {}) for {} every {} minutes", item->config.name,
                 item->config.id, item->config.targetAddress, item->config.intervalMinutes);

    if (running_ && item->config.enabled) {
        scheduleNextScan(item);
    }
}

void ScheduledPortScanner::updateSchedule(const core::ScheduledScanConfig& config) {
    std::lock_guard lock(mutex_);

    auto it = schedules_.find(config.id);
    if (it == schedules_.end()) {
        spdlog::warn("Schedule not found for update: {}", config.id);
        return;
    }

    auto& item = it->second;
    item->active = false;
    if (item->timer) {
        item->timer->cancel();
    }

    item->config = config;
    item->active = config.enabled && running_;

    spdlog::info("Updated scheduled scan: {} (ID: {})", config.name, config.id);

    if (running_ && item->config.enabled) {
        scheduleNextScan(item);
    }
}

void ScheduledPortScanner::removeSchedule(int64_t scheduleId) {
    std::lock_guard lock(mutex_);

    auto it = schedules_.find(scheduleId);
    if (it == schedules_.end()) {
        return;
    }

    it->second->active = false;
    if (it->second->timer) {
        it->second->timer->cancel();
    }
    schedules_.erase(it);

    spdlog::info("Removed scheduled scan: {}", scheduleId);
}

void ScheduledPortScanner::enableSchedule(int64_t scheduleId, bool enabled) {
    std::lock_guard lock(mutex_);

    auto it = schedules_.find(scheduleId);
    if (it == schedules_.end()) {
        return;
    }

    auto& item = it->second;
    item->config.enabled = enabled;

    if (!enabled) {
        item->active = false;
        if (item->timer) {
            item->timer->cancel();
        }
    } else if (running_) {
        item->active = true;
        scheduleNextScan(item);
    }

    spdlog::info("Schedule {} {}", scheduleId, enabled ? "enabled" : "disabled");
}

std::vector<core::ScheduledScanConfig> ScheduledPortScanner::getSchedules() const {
    std::lock_guard lock(mutex_);

    std::vector<core::ScheduledScanConfig> result;
    result.reserve(schedules_.size());
    for (const auto& [id, item] : schedules_) {
        result.push_back(item->config);
    }
    return result;
}

std::optional<core::ScheduledScanConfig> ScheduledPortScanner::getSchedule(int64_t scheduleId) const {
    std::lock_guard lock(mutex_);

    auto it = schedules_.find(scheduleId);
    if (it == schedules_.end()) {
        return std::nullopt;
    }
    return it->second->config;
}

void ScheduledPortScanner::runNow(int64_t scheduleId) {
    std::shared_ptr<ScheduledItem> item;
    {
        std::lock_guard lock(mutex_);
        auto it = schedules_.find(scheduleId);
        if (it == schedules_.end()) {
            spdlog::warn("Schedule not found for immediate run: {}", scheduleId);
            return;
        }
        item = it->second;
    }

    spdlog::info("Running scheduled scan immediately: {} (ID: {})", item->config.name, scheduleId);
    executeScan(item);
}

void ScheduledPortScanner::start() {
    if (running_.exchange(true)) {
        return;
    }

    std::lock_guard lock(mutex_);
    for (auto& [id, item] : schedules_) {
        if (item->config.enabled) {
            item->active = true;
            scheduleNextScan(item);
        }
    }

    spdlog::info("ScheduledPortScanner started with {} schedules", schedules_.size());
}

void ScheduledPortScanner::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    std::lock_guard lock(mutex_);
    for (auto& [id, item] : schedules_) {
        item->active = false;
        if (item->timer) {
            item->timer->cancel();
        }
    }

    spdlog::info("ScheduledPortScanner stopped");
}

bool ScheduledPortScanner::isRunning() const {
    return running_;
}

void ScheduledPortScanner::setScanCompleteCallback(ScanCompleteCallback callback) {
    std::lock_guard lock(mutex_);
    scanCompleteCallback_ = std::move(callback);
}

void ScheduledPortScanner::setDiffCallback(DiffCallback callback) {
    std::lock_guard lock(mutex_);
    diffCallback_ = std::move(callback);
}

void ScheduledPortScanner::scheduleNextScan(std::shared_ptr<ScheduledItem> item) {
    if (!item->active || !running_) {
        return;
    }

    auto nextRun = std::chrono::minutes(item->config.intervalMinutes);
    item->config.nextRunAt = std::chrono::system_clock::now() + nextRun;

    item->timer->expires_after(nextRun);
    item->timer->async_wait([this, item](const asio::error_code& ec) {
        if (ec || !item->active || !running_) {
            return;
        }

        executeScan(item);
        scheduleNextScan(item);
    });

    spdlog::debug("Next scan for {} scheduled in {} minutes", item->config.name,
                  item->config.intervalMinutes);
}

void ScheduledPortScanner::executeScan(std::shared_ptr<ScheduledItem> item) {
    if (portScanner_.isScanning()) {
        spdlog::warn("Port scanner is busy, skipping scheduled scan: {}", item->config.name);
        return;
    }

    auto scanConfig = item->config.toPortScanConfig();
    auto scheduleId = item->config.id;
    auto targetAddress = item->config.targetAddress;

    spdlog::info("Starting scheduled scan: {} for {}", item->config.name, targetAddress);

    auto results = std::make_shared<std::vector<core::PortScanResult>>();

    portScanner_.scanAsync(
        scanConfig,
        [results](const core::PortScanResult& result) {
            results->push_back(result);
        },
        [](const core::PortScanProgress& /*progress*/) {},
        [this, item, scheduleId, targetAddress, results](const std::vector<core::PortScanResult>&) {
            std::lock_guard lock(mutex_);

            item->config.lastRunAt = std::chrono::system_clock::now();

            auto previousResults = item->lastResults;
            item->lastResults = *results;

            spdlog::info("Scheduled scan complete: {} - {} ports scanned", item->config.name,
                         results->size());

            if (scanCompleteCallback_) {
                scanCompleteCallback_(scheduleId, *results);
            }

            if (!previousResults.empty() && diffCallback_) {
                auto diff = computeDiff(targetAddress, previousResults, *results);
                if (diff.hasChanges()) {
                    spdlog::info("Detected {} port changes for {}", diff.changes.size(),
                                 targetAddress);
                    diffCallback_(scheduleId, diff);
                }
            }
        });
}

core::PortScanDiff ScheduledPortScanner::computeDiff(
    const std::string& targetAddress, const std::vector<core::PortScanResult>& previous,
    const std::vector<core::PortScanResult>& current) {

    core::PortScanDiff diff;
    diff.targetAddress = targetAddress;
    diff.totalPortsScanned = static_cast<int>(current.size());

    if (!previous.empty()) {
        diff.previousScanTime = previous.front().scanTimestamp;
    }
    if (!current.empty()) {
        diff.currentScanTime = current.front().scanTimestamp;
    }

    std::unordered_map<uint16_t, core::PortScanResult> prevMap;
    for (const auto& result : previous) {
        prevMap[result.port] = result;
        if (result.state == core::PortState::Open) {
            diff.openPortsBefore++;
        }
    }

    std::unordered_map<uint16_t, core::PortScanResult> currMap;
    for (const auto& result : current) {
        currMap[result.port] = result;
        if (result.state == core::PortState::Open) {
            diff.openPortsAfter++;
        }
    }

    for (const auto& [port, currResult] : currMap) {
        auto prevIt = prevMap.find(port);
        if (prevIt == prevMap.end()) {
            if (currResult.state == core::PortState::Open) {
                core::PortChange change;
                change.port = port;
                change.changeType = core::PortChangeType::NewOpen;
                change.previousState = core::PortState::Unknown;
                change.currentState = currResult.state;
                change.serviceName = currResult.serviceName;
                diff.changes.push_back(change);
            }
        } else if (prevIt->second.state != currResult.state) {
            core::PortChange change;
            change.port = port;
            change.previousState = prevIt->second.state;
            change.currentState = currResult.state;
            change.serviceName = currResult.serviceName;

            if (currResult.state == core::PortState::Open &&
                prevIt->second.state != core::PortState::Open) {
                change.changeType = core::PortChangeType::NewOpen;
            } else if (currResult.state != core::PortState::Open &&
                       prevIt->second.state == core::PortState::Open) {
                change.changeType = core::PortChangeType::NewClosed;
            } else {
                change.changeType = core::PortChangeType::StateChanged;
            }

            diff.changes.push_back(change);
        }
    }

    for (const auto& [port, prevResult] : prevMap) {
        if (currMap.find(port) == currMap.end()) {
            if (prevResult.state == core::PortState::Open) {
                core::PortChange change;
                change.port = port;
                change.changeType = core::PortChangeType::NewClosed;
                change.previousState = prevResult.state;
                change.currentState = core::PortState::Unknown;
                change.serviceName = prevResult.serviceName;
                diff.changes.push_back(change);
            }
        }
    }

    std::sort(diff.changes.begin(), diff.changes.end(),
              [](const core::PortChange& a, const core::PortChange& b) { return a.port < b.port; });

    return diff;
}

void ScheduledPortScanner::setLastScanResults(int64_t scheduleId,
                                               std::vector<core::PortScanResult> results) {
    std::lock_guard lock(mutex_);

    auto it = schedules_.find(scheduleId);
    if (it != schedules_.end()) {
        it->second->lastResults = std::move(results);
    }
}

std::vector<core::PortScanResult> ScheduledPortScanner::getLastScanResults(int64_t scheduleId) const {
    std::lock_guard lock(mutex_);

    auto it = schedules_.find(scheduleId);
    if (it != schedules_.end()) {
        return it->second->lastResults;
    }
    return {};
}

} // namespace netpulse::infra
