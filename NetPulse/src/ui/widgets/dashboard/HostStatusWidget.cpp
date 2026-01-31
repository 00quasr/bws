#include "ui/widgets/dashboard/HostStatusWidget.hpp"

#include "app/Application.hpp"
#include "core/types/Host.hpp"

#include <QVBoxLayout>

namespace netpulse::ui {

HostStatusWidget::HostStatusWidget(QWidget* parent)
    : DashboardWidget("Host Status", parent) {
    auto* contentWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    hostList_ = new QListWidget(this);
    hostList_->setStyleSheet(R"(
        QListWidget {
            border: none;
            background: transparent;
        }
        QListWidget::item {
            padding: 4px 8px;
            border-radius: 4px;
        }
        QListWidget::item:hover {
            background: palette(midlight);
        }
    )");

    connect(hostList_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        int64_t hostId = item->data(Qt::UserRole).toLongLong();
        emit hostClicked(hostId);
    });

    layout->addWidget(hostList_);
    setContentWidget(contentWidget);

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &HostStatusWidget::refresh);
    refreshTimer_->start(2000);

    refresh();
}

nlohmann::json HostStatusWidget::settings() const {
    return {{"showOnlyDown", showOnlyDown_}};
}

void HostStatusWidget::applySettings(const nlohmann::json& settings) {
    showOnlyDown_ = settings.value("showOnlyDown", false);
    setTitle(showOnlyDown_ ? "Hosts Down" : "Host Status");
    refresh();
}

void HostStatusWidget::refresh() {
    hostList_->clear();

    auto& vm = app::Application::instance().dashboardViewModel();
    auto hosts = vm.getHosts();

    for (const auto& host : hosts) {
        if (!host.enabled)
            continue;

        if (showOnlyDown_ && host.status != core::HostStatus::Down)
            continue;

        auto* item = new QListWidgetItem(hostList_);
        item->setData(Qt::UserRole, QVariant::fromValue(host.id));

        QString statusIcon;
        QString statusColor;
        switch (host.status) {
        case core::HostStatus::Up:
            statusIcon = "●";
            statusColor = "#27ae60";
            break;
        case core::HostStatus::Warning:
            statusIcon = "●";
            statusColor = "#e67e22";
            break;
        case core::HostStatus::Down:
            statusIcon = "●";
            statusColor = "#e74c3c";
            break;
        default:
            statusIcon = "○";
            statusColor = "#95a5a6";
            break;
        }

        QString displayText = QString("<span style='color: %1;'>%2</span> %3")
                                  .arg(statusColor, statusIcon,
                                       QString::fromStdString(host.name));

        auto* label = new QLabel(displayText, hostList_);
        label->setTextFormat(Qt::RichText);

        item->setSizeHint(label->sizeHint() + QSize(8, 4));
        hostList_->setItemWidget(item, label);
    }

    if (hostList_->count() == 0) {
        auto* item = new QListWidgetItem(showOnlyDown_ ? "All hosts are up" : "No hosts configured",
                                         hostList_);
        item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        item->setForeground(Qt::gray);
    }
}

} // namespace netpulse::ui
