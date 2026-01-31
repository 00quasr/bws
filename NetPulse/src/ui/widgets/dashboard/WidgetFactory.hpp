#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <memory>

namespace netpulse::ui {

class WidgetFactory {
public:
    static std::unique_ptr<DashboardWidget> create(WidgetType type, QWidget* parent = nullptr);
    static std::unique_ptr<DashboardWidget> createFromConfig(const WidgetConfig& config,
                                                              QWidget* parent = nullptr);
};

} // namespace netpulse::ui
