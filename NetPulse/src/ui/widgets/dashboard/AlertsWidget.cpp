#include "ui/widgets/dashboard/AlertsWidget.hpp"

#include "app/Application.hpp"
#include "core/types/Alert.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace netpulse::ui {

AlertsWidget::AlertsWidget(QWidget* parent) : DashboardWidget("Recent Alerts", parent) {
    auto* contentWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    setupFilterBar();
    layout->addWidget(searchEdit_);

    auto* filterRow = new QWidget(this);
    auto* filterLayout = new QHBoxLayout(filterRow);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(4);
    filterLayout->addWidget(severityCombo_);
    filterLayout->addWidget(typeCombo_);
    filterLayout->addWidget(statusCombo_);
    filterLayout->addStretch();
    layout->addWidget(filterRow);

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

    searchDebounceTimer_ = new QTimer(this);
    searchDebounceTimer_->setSingleShot(true);
    connect(searchDebounceTimer_, &QTimer::timeout, this, &AlertsWidget::refresh);

    auto& alertsVm = app::Application::instance().alertsViewModel();
    connect(&alertsVm, &viewmodels::AlertsViewModel::alertTriggered, this,
            &AlertsWidget::refresh);

    refresh();
}

void AlertsWidget::setupFilterBar() {
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setPlaceholderText("Search alerts...");
    searchEdit_->setClearButtonEnabled(true);
    searchEdit_->setStyleSheet(R"(
        QLineEdit {
            padding: 4px 8px;
            border: 1px solid palette(mid);
            border-radius: 4px;
        }
        QLineEdit:focus {
            border-color: palette(highlight);
        }
    )");
    connect(searchEdit_, &QLineEdit::textChanged, this, &AlertsWidget::onSearchTextChanged);

    severityCombo_ = new QComboBox(this);
    severityCombo_->addItem("All Severities", -1);
    severityCombo_->addItem("Critical", static_cast<int>(core::AlertSeverity::Critical));
    severityCombo_->addItem("Warning", static_cast<int>(core::AlertSeverity::Warning));
    severityCombo_->addItem("Info", static_cast<int>(core::AlertSeverity::Info));
    severityCombo_->setMinimumWidth(100);
    connect(severityCombo_, &QComboBox::currentIndexChanged, this, &AlertsWidget::onFilterChanged);

    typeCombo_ = new QComboBox(this);
    typeCombo_->addItem("All Types", -1);
    typeCombo_->addItem("Host Down", static_cast<int>(core::AlertType::HostDown));
    typeCombo_->addItem("High Latency", static_cast<int>(core::AlertType::HighLatency));
    typeCombo_->addItem("Packet Loss", static_cast<int>(core::AlertType::PacketLoss));
    typeCombo_->addItem("Host Recovered", static_cast<int>(core::AlertType::HostRecovered));
    typeCombo_->addItem("Scan Complete", static_cast<int>(core::AlertType::ScanComplete));
    typeCombo_->setMinimumWidth(110);
    connect(typeCombo_, &QComboBox::currentIndexChanged, this, &AlertsWidget::onFilterChanged);

    statusCombo_ = new QComboBox(this);
    statusCombo_->addItem("All Status", -1);
    statusCombo_->addItem("Active", 0);
    statusCombo_->addItem("Acknowledged", 1);
    statusCombo_->setMinimumWidth(100);
    connect(statusCombo_, &QComboBox::currentIndexChanged, this, &AlertsWidget::onFilterChanged);
}

void AlertsWidget::onFilterChanged() {
    refresh();
}

void AlertsWidget::onSearchTextChanged(const QString& text) {
    (void)text;
    searchDebounceTimer_->start(300);
}

core::AlertFilter AlertsWidget::buildFilter() const {
    core::AlertFilter filter;

    int severityVal = severityCombo_->currentData().toInt();
    if (severityVal >= 0) {
        filter.severity = static_cast<core::AlertSeverity>(severityVal);
    }

    int typeVal = typeCombo_->currentData().toInt();
    if (typeVal >= 0) {
        filter.type = static_cast<core::AlertType>(typeVal);
    }

    int statusVal = statusCombo_->currentData().toInt();
    if (statusVal >= 0) {
        filter.acknowledged = (statusVal == 1);
    }

    filter.searchText = searchEdit_->text().toStdString();

    return filter;
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
    auto filter = buildFilter();

    std::vector<core::Alert> alerts;
    if (filter.isEmpty()) {
        alerts = vm.getRecentAlerts(maxAlerts_);
    } else {
        alerts = vm.getFilteredAlerts(filter, maxAlerts_);
    }

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
                .arg(severityColor, severityIcon, QString::fromStdString(alert.title), timeStr,
                     alert.acknowledged ? "acknowledged" : "active");

        auto* label = new QLabel(displayText, alertList_);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);

        item->setSizeHint(label->sizeHint() + QSize(8, 8));
        alertList_->setItemWidget(item, label);
    }

    if (alertList_->count() == 0) {
        auto* item = new QListWidgetItem("No alerts matching filter", alertList_);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        item->setForeground(Qt::gray);
    }
}

} // namespace netpulse::ui
