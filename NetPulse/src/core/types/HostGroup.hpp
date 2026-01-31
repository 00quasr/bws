#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace netpulse::core {

struct HostGroup {
    int64_t id{0};
    std::string name;
    std::string description;
    std::optional<int64_t> parentId;
    std::chrono::system_clock::time_point createdAt;

    [[nodiscard]] bool isValid() const;

    bool operator==(const HostGroup& other) const = default;
};

} // namespace netpulse::core
