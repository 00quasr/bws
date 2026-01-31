#include "ui/widgets/dashboard/WidgetFactory.hpp"

#include "ui/widgets/dashboard/AlertsWidget.hpp"
#include "ui/widgets/dashboard/HostStatusWidget.hpp"
#include "ui/widgets/dashboard/LatencyHistoryWidget.hpp"
#include "ui/widgets/dashboard/NetworkOverviewWidget.hpp"
#include "ui/widgets/dashboard/StatisticsWidget.hpp"

namespace netpulse::ui {

std::unique_ptr<DashboardWidget> WidgetFactory::create(WidgetType type, QWidget* parent) {
    switch (type) {
    case WidgetType::Statistics:
        return std::make_unique<StatisticsWidget>(parent);
    case WidgetType::HostStatus:
        return std::make_unique<HostStatusWidget>(parent);
    case WidgetType::Alerts:
        return std::make_unique<AlertsWidget>(parent);
    case WidgetType::NetworkOverview:
        return std::make_unique<NetworkOverviewWidget>(parent);
    case WidgetType::LatencyHistory:
        return std::make_unique<LatencyHistoryWidget>(parent);
    }
    return std::make_unique<StatisticsWidget>(parent);
}

std::unique_ptr<DashboardWidget> WidgetFactory::createFromConfig(const WidgetConfig& config,
                                                                  QWidget* parent) {
    auto widget = create(config.type, parent);
    widget->setTitle(config.title);
    widget->applySettings(config.settings);
    return widget;
}

} // namespace netpulse::ui
