#include "core/types/HostGroup.hpp"

namespace netpulse::core {

bool HostGroup::isValid() const {
    return !name.empty();
}

} // namespace netpulse::core
