#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QListWidget>
#include <QTimer>

namespace netpulse::ui {

class AlertsWidget : public DashboardWidget {
    Q_OBJECT

public:
    explicit AlertsWidget(QWidget* parent = nullptr);

    [[nodiscard]] WidgetType widgetType() const override { return WidgetType::Alerts; }

    [[nodiscard]] nlohmann::json settings() const override;
    void applySettings(const nlohmann::json& settings) override;

    void refresh() override;

signals:
    void alertClicked(int64_t alertId);

private:
    QListWidget* alertList_{nullptr};
    QTimer* refreshTimer_{nullptr};
    int maxAlerts_{10};
};

} // namespace netpulse::ui
