#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QLabel>
#include <QTimer>

namespace netpulse::ui {

class StatisticsWidget : public DashboardWidget {
    Q_OBJECT

public:
    explicit StatisticsWidget(QWidget* parent = nullptr);

    [[nodiscard]] WidgetType widgetType() const override { return WidgetType::Statistics; }

    void refresh() override;

private:
    QLabel* hostsUpLabel_{nullptr};
    QLabel* hostsDownLabel_{nullptr};
    QLabel* avgLatencyLabel_{nullptr};
    QLabel* packetLossLabel_{nullptr};

    QTimer* refreshTimer_{nullptr};
};

} // namespace netpulse::ui
