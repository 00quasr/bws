#include "ui/widgets/dashboard/StatisticsWidget.hpp"

#include "app/Application.hpp"

#include <QGridLayout>

namespace netpulse::ui {

StatisticsWidget::StatisticsWidget(QWidget* parent)
    : DashboardWidget("Network Statistics", parent) {
    auto* contentWidget = new QWidget(this);
    auto* grid = new QGridLayout(contentWidget);
    grid->setContentsMargins(4, 4, 4, 4);
    grid->setSpacing(8);

    auto createStatLabel = [this](const QString& name, int row, QGridLayout* layout) {
        auto* nameLabel = new QLabel(name + ":", this);
        nameLabel->setStyleSheet("color: palette(text); font-size: 11px;");

        auto* valueLabel = new QLabel("--", this);
        valueLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        layout->addWidget(nameLabel, row, 0);
        layout->addWidget(valueLabel, row, 1);
        return valueLabel;
    };

    hostsUpLabel_ = createStatLabel("Hosts Up", 0, grid);
    hostsDownLabel_ = createStatLabel("Hosts Down", 1, grid);
    avgLatencyLabel_ = createStatLabel("Avg Latency", 2, grid);
    packetLossLabel_ = createStatLabel("Packet Loss", 3, grid);

    grid->setRowStretch(4, 1);
    setContentWidget(contentWidget);

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &StatisticsWidget::refresh);
    refreshTimer_->start(1000);

    refresh();
}

void StatisticsWidget::refresh() {
    auto& vm = app::Application::instance().dashboardViewModel();

    int up = vm.hostsUp();
    int down = vm.hostsDown();

    hostsUpLabel_->setText(QString::number(up));
    hostsUpLabel_->setStyleSheet(
        QString("font-weight: bold; font-size: 14px; color: %1;")
            .arg(up > 0 ? "#27ae60" : "palette(text)"));

    hostsDownLabel_->setText(QString::number(down));
    hostsDownLabel_->setStyleSheet(
        QString("font-weight: bold; font-size: 14px; color: %1;")
            .arg(down > 0 ? "#e74c3c" : "palette(text)"));

    auto hosts = vm.getHosts();
    double totalLatency = 0.0;
    double totalPacketLoss = 0.0;
    int hostCount = 0;

    for (const auto& host : hosts) {
        if (!host.enabled)
            continue;

        auto stats = vm.getStatistics(host.id);
        if (stats.totalPings > 0) {
            totalLatency +=
                static_cast<double>(stats.avgLatency.count()) / 1000.0;
            totalPacketLoss += stats.packetLossPercent;
            hostCount++;
        }
    }

    if (hostCount > 0) {
        double avgLatency = totalLatency / hostCount;
        double avgPacketLoss = totalPacketLoss / hostCount;

        avgLatencyLabel_->setText(QString("%1 ms").arg(avgLatency, 0, 'f', 1));
        packetLossLabel_->setText(QString("%1%").arg(avgPacketLoss, 0, 'f', 1));

        QString latencyColor = avgLatency > 100 ? "#e67e22" : "#27ae60";
        avgLatencyLabel_->setStyleSheet(
            QString("font-weight: bold; font-size: 14px; color: %1;")
                .arg(latencyColor));

        QString lossColor = avgPacketLoss > 5 ? "#e74c3c" : "#27ae60";
        packetLossLabel_->setStyleSheet(
            QString("font-weight: bold; font-size: 14px; color: %1;")
                .arg(lossColor));
    } else {
        avgLatencyLabel_->setText("--");
        packetLossLabel_->setText("--");
        avgLatencyLabel_->setStyleSheet(
            "font-weight: bold; font-size: 14px; color: palette(text);");
        packetLossLabel_->setStyleSheet(
            "font-weight: bold; font-size: 14px; color: palette(text);");
    }
}

} // namespace netpulse::ui
