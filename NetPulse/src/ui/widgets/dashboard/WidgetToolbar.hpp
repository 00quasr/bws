#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QToolBar>

namespace netpulse::ui {

class WidgetToolbar : public QToolBar {
    Q_OBJECT

public:
    explicit WidgetToolbar(QWidget* parent = nullptr);

signals:
    void addWidgetRequested(WidgetType type);
    void resetLayoutRequested();

private:
    void setupActions();
};

} // namespace netpulse::ui
