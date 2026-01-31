#include "ui/widgets/HostListWidget.hpp"

#include "app/Application.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace netpulse::ui {

HostListWidget::HostListWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshHosts();
}

void HostListWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* headerLabel = new QLabel("<b>Monitored Hosts</b>", this);
    headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(headerLabel);

    listWidget_ = new QListWidget(this);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    listWidget_->setSpacing(2);
    layout->addWidget(listWidget_);

    connect(listWidget_, &QListWidget::itemSelectionChanged, this,
            &HostListWidget::onItemSelectionChanged);
    connect(listWidget_, &QListWidget::itemDoubleClicked, this,
            &HostListWidget::onItemDoubleClicked);
}

void HostListWidget::refreshHosts() {
    listWidget_->clear();
    hostItems_.clear();
    statusIndicators_.clear();

    auto& vm = app::Application::instance().hostMonitorViewModel();
    auto hosts = vm.getAllHosts();

    for (const auto& host : hosts) {
        auto* item = new QListWidgetItem(listWidget_);
        item->setData(Qt::UserRole, QVariant::fromValue(host.id));
        item->setSizeHint(QSize(0, 50));

        auto* widget = createHostItemWidget(host);
        listWidget_->setItemWidget(item, widget);

        hostItems_[host.id] = item;
    }
}

void HostListWidget::updateHostStatus(int64_t hostId) {
    auto it = statusIndicators_.find(hostId);
    if (it == statusIndicators_.end()) {
        return;
    }

    auto& vm = app::Application::instance().hostMonitorViewModel();
    auto host = vm.getHost(hostId);

    if (host) {
        it->second->setStatus(host->status);
    }
}

int64_t HostListWidget::selectedHostId() const {
    auto items = listWidget_->selectedItems();
    if (items.isEmpty()) {
        return -1;
    }
    return items.first()->data(Qt::UserRole).toLongLong();
}

void HostListWidget::onItemSelectionChanged() {
    emit hostSelected(selectedHostId());
}

void HostListWidget::onItemDoubleClicked(QListWidgetItem* item) {
    if (item) {
        emit hostDoubleClicked(item->data(Qt::UserRole).toLongLong());
    }
}

QWidget* HostListWidget::createHostItemWidget(const core::Host& host) {
    auto* widget = new QWidget(this);
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(5, 5, 5, 5);

    // Status indicator
    auto* indicator = new StatusIndicator(this);
    indicator->setStatus(host.status);
    indicator->setFixedSize(20, 20);
    layout->addWidget(indicator);
    statusIndicators_[host.id] = indicator;

    // Host info
    auto* infoWidget = new QWidget(widget);
    auto* infoLayout = new QVBoxLayout(infoWidget);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(0);

    auto* nameLabel = new QLabel(QString::fromStdString(host.name), infoWidget);
    nameLabel->setStyleSheet("font-weight: bold;");
    infoLayout->addWidget(nameLabel);

    auto* addressLabel = new QLabel(QString::fromStdString(host.address), infoWidget);
    addressLabel->setStyleSheet("color: gray; font-size: 10px;");
    infoLayout->addWidget(addressLabel);

    layout->addWidget(infoWidget, 1);

    // Status text
    auto* statusLabel = new QLabel(QString::fromStdString(host.statusToString()), widget);
    switch (host.status) {
    case core::HostStatus::Up:
        statusLabel->setStyleSheet("color: green;");
        break;
    case core::HostStatus::Warning:
        statusLabel->setStyleSheet("color: orange;");
        break;
    case core::HostStatus::Down:
        statusLabel->setStyleSheet("color: red;");
        break;
    default:
        statusLabel->setStyleSheet("color: gray;");
        break;
    }
    layout->addWidget(statusLabel);

    return widget;
}

} // namespace netpulse::ui
