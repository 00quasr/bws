#include "ui/widgets/dashboard/AlertsWidget.hpp"

#include "app/Application.hpp"
#include "core/types/Alert.hpp"

#include <QVBoxLayout>

namespace netpulse::ui {

AlertsWidget::AlertsWidget(QWidget* parent)
    : DashboardWidget("Recent Alerts", parent) {
    auto* contentWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    alertList_ = new QListWidget(this);
    alertList_->setStyleSheet(R"(
        QListWidget {
            border: none;
            background: transparent;
        }
        QListWidget::item {
            padding: 6px 8px;
            border-radius: 4px;
            margin-bottom: 2px;
        }
        QListWidget::item:hover {
            background: palette(midlight);
        }
    )");

    connect(alertList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        int64_t alertId = item->data(Qt::UserRole).toLongLong();
        emit alertClicked(alertId);
    });

    layout->addWidget(alertList_);
    setContentWidget(contentWidget);

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &AlertsWidget::refresh);
    refreshTimer_->start(2000);

    auto& alertsVm = app::Application::instance().alertsViewModel();
    connect(&alertsVm, &viewmodels::AlertsViewModel::alertTriggered, this,
            &AlertsWidget::refresh);

    refresh();
}

nlohmann::json AlertsWidget::settings() const {
    return {{"maxAlerts", maxAlerts_}};
}

void AlertsWidget::applySettings(const nlohmann::json& settings) {
    maxAlerts_ = settings.value("maxAlerts", 10);
    refresh();
}

void AlertsWidget::refresh() {
    alertList_->clear();

    auto& vm = app::Application::instance().alertsViewModel();
    auto alerts = vm.getRecentAlerts(maxAlerts_);

    for (const auto& alert : alerts) {
        auto* item = new QListWidgetItem(alertList_);
        item->setData(Qt::UserRole, QVariant::fromValue(alert.id));

        QString severityIcon;
        QString severityColor;
        switch (alert.severity) {
        case core::AlertSeverity::Critical:
            severityIcon = "⚠";
            severityColor = "#e74c3c";
            break;
        case core::AlertSeverity::Warning:
            severityIcon = "⚡";
            severityColor = "#e67e22";
            break;
        default:
            severityIcon = "ℹ";
            severityColor = "#3498db";
            break;
        }

        auto timestamp = std::chrono::system_clock::to_time_t(alert.timestamp);
        std::tm tm = *std::localtime(&timestamp);
        char timeStr[32];
        std::strftime(timeStr, sizeof(timeStr), "%H:%M", &tm);

        QString displayText =
            QString("<div style='margin-bottom: 2px;'>"
                    "<span style='color: %1; font-size: 12px;'>%2</span> "
                    "<span style='font-weight: bold;'>%3</span>"
                    "</div>"
                    "<div style='color: gray; font-size: 10px;'>%4 • %5</div>")
                .arg(severityColor, severityIcon,
                     QString::fromStdString(alert.title), timeStr,
                     alert.acknowledged ? "acknowledged" : "active");

        auto* label = new QLabel(displayText, alertList_);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);

        item->setSizeHint(label->sizeHint() + QSize(8, 8));
        alertList_->setItemWidget(item, label);
    }

    if (alertList_->count() == 0) {
        auto* item = new QListWidgetItem("No recent alerts", alertList_);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        item->setForeground(Qt::gray);
    }
}

} // namespace netpulse::ui
