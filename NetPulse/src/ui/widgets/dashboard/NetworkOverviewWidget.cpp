#include "ui/widgets/dashboard/NetworkOverviewWidget.hpp"

#include "app/Application.hpp"
#include "core/types/NetworkInterface.hpp"

#include <QScrollArea>

namespace netpulse::ui {

NetworkOverviewWidget::NetworkOverviewWidget(QWidget* parent)
    : DashboardWidget("Network Interfaces", parent) {
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("background: transparent;");

    auto* contentWidget = new QWidget(scrollArea);
    interfacesLayout_ = new QVBoxLayout(contentWidget);
    interfacesLayout_->setContentsMargins(0, 0, 0, 0);
    interfacesLayout_->setSpacing(8);
    interfacesLayout_->addStretch();

    scrollArea->setWidget(contentWidget);
    setContentWidget(scrollArea);

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &NetworkOverviewWidget::refresh);
    refreshTimer_->start(2000);

    refresh();
}

void NetworkOverviewWidget::refresh() {
    while (interfacesLayout_->count() > 1) {
        auto* item = interfacesLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }

    auto interfaces = core::NetworkInterfaceEnumerator::enumerate();

    for (const auto& iface : interfaces) {
        if (iface.isLoopback)
            continue;

        auto* card = new QFrame(this);
        card->setStyleSheet(R"(
            QFrame {
                background: palette(alternateBase);
                border-radius: 4px;
                padding: 6px;
            }
        )");

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(8, 6, 8, 6);
        cardLayout->setSpacing(4);

        auto* nameLabel =
            new QLabel(QString("<b>%1</b>").arg(QString::fromStdString(iface.name)), card);
        nameLabel->setTextFormat(Qt::RichText);
        cardLayout->addWidget(nameLabel);

        if (!iface.ipAddress.empty()) {
            auto* ipLabel = new QLabel(
                QString("<span style='color: gray; font-size: 10px;'>%1</span>")
                    .arg(QString::fromStdString(iface.ipAddress)),
                card);
            ipLabel->setTextFormat(Qt::RichText);
            cardLayout->addWidget(ipLabel);
        }

        auto* rxLabel = new QLabel(
            QString("↓ %1").arg(QString::fromStdString(iface.formatBytesReceived())), card);
        rxLabel->setStyleSheet("color: #27ae60; font-size: 11px;");
        cardLayout->addWidget(rxLabel);

        auto* txLabel =
            new QLabel(QString("↑ %1").arg(QString::fromStdString(iface.formatBytesSent())),
                       card);
        txLabel->setStyleSheet("color: #3498db; font-size: 11px;");
        cardLayout->addWidget(txLabel);

        interfacesLayout_->insertWidget(interfacesLayout_->count() - 1, card);
    }

    if (interfacesLayout_->count() == 1) {
        auto* noInterfacesLabel = new QLabel("No network interfaces found", this);
        noInterfacesLabel->setStyleSheet("color: gray;");
        noInterfacesLabel->setAlignment(Qt::AlignCenter);
        interfacesLayout_->insertWidget(0, noInterfacesLabel);
    }
}

} // namespace netpulse::ui
