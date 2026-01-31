#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QListWidget>
#include <QTimer>

namespace netpulse::ui {

class HostStatusWidget : public DashboardWidget {
    Q_OBJECT

public:
    explicit HostStatusWidget(QWidget* parent = nullptr);

    [[nodiscard]] WidgetType widgetType() const override { return WidgetType::HostStatus; }

    [[nodiscard]] nlohmann::json settings() const override;
    void applySettings(const nlohmann::json& settings) override;

    void refresh() override;

signals:
    void hostClicked(int64_t hostId);

private:
    QListWidget* hostList_{nullptr};
    QTimer* refreshTimer_{nullptr};
    bool showOnlyDown_{false};
};

} // namespace netpulse::ui
