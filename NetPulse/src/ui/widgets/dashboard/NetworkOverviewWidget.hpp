#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QVBoxLayout>

namespace netpulse::ui {

class NetworkOverviewWidget : public DashboardWidget {
    Q_OBJECT

public:
    explicit NetworkOverviewWidget(QWidget* parent = nullptr);

    [[nodiscard]] WidgetType widgetType() const override { return WidgetType::NetworkOverview; }

    void refresh() override;

private:
    QVBoxLayout* interfacesLayout_{nullptr};
    QTimer* refreshTimer_{nullptr};
};

} // namespace netpulse::ui
