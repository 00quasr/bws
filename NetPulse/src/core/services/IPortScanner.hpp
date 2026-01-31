#pragma once

#include "core/types/PortScanResult.hpp"

#include <functional>
#include <future>
#include <vector>

namespace netpulse::core {

struct PortScanProgress {
    int totalPorts{0};
    int scannedPorts{0};
    int openPorts{0};
    bool cancelled{false};

    [[nodiscard]] double percentComplete() const {
        return totalPorts > 0 ? (static_cast<double>(scannedPorts) / totalPorts) * 100.0 : 0.0;
    }
};

class IPortScanner {
public:
    using ProgressCallback = std::function<void(const PortScanProgress&)>;
    using ResultCallback = std::function<void(const PortScanResult&)>;
    using CompletionCallback = std::function<void(const std::vector<PortScanResult>&)>;

    virtual ~IPortScanner() = default;

    virtual void scanAsync(const PortScanConfig& config, ResultCallback onResult,
                           ProgressCallback onProgress, CompletionCallback onComplete) = 0;

    virtual void cancel() = 0;
    virtual bool isScanning() const = 0;
};

} // namespace netpulse::core
