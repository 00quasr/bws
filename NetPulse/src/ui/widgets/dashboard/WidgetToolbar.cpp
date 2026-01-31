#include "ui/widgets/dashboard/WidgetToolbar.hpp"

#include <QMenu>
#include <QToolButton>

namespace netpulse::ui {

WidgetToolbar::WidgetToolbar(QWidget* parent) : QToolBar(parent) {
    setMovable(false);
    setupActions();
}

void WidgetToolbar::setupActions() {
    auto* addButton = new QToolButton(this);
    addButton->setText("Add Widget");
    addButton->setPopupMode(QToolButton::InstantPopup);

    auto* addMenu = new QMenu(addButton);

    addMenu->addAction("Statistics", this, [this]() {
        emit addWidgetRequested(WidgetType::Statistics);
    });

    addMenu->addAction("Host Status", this, [this]() {
        emit addWidgetRequested(WidgetType::HostStatus);
    });

    addMenu->addAction("Alerts", this, [this]() {
        emit addWidgetRequested(WidgetType::Alerts);
    });

    addMenu->addAction("Network Overview", this, [this]() {
        emit addWidgetRequested(WidgetType::NetworkOverview);
    });

    addMenu->addAction("Latency History", this, [this]() {
        emit addWidgetRequested(WidgetType::LatencyHistory);
    });

    addButton->setMenu(addMenu);
    addWidget(addButton);

    addSeparator();

    addAction("Reset Layout", this, &WidgetToolbar::resetLayoutRequested);
}

} // namespace netpulse::ui
