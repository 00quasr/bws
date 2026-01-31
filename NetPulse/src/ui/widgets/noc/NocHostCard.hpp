#pragma once

#include "core/types/Host.hpp"

#include <QFrame>
#include <QLabel>

namespace netpulse::ui {

class NocHostCard : public QFrame {
    Q_OBJECT

public:
    explicit NocHostCard(const core::Host& host, QWidget* parent = nullptr);

    void updateStatus(core::HostStatus status, double latencyMs);
    [[nodiscard]] int64_t hostId() const { return hostId_; }

private:
    int64_t hostId_;
    QLabel* nameLabel_{nullptr};
    QLabel* statusLabel_{nullptr};
    QLabel* latencyLabel_{nullptr};
    QFrame* statusIndicator_{nullptr};
};

} // namespace netpulse::ui
