#include "ui/widgets/noc/NocHostCard.hpp"

#include <QStyle>
#include <QVBoxLayout>

namespace netpulse::ui {

NocHostCard::NocHostCard(const core::Host& host, QWidget* parent)
    : QFrame(parent), hostId_(host.id) {
    setObjectName("NocHostCard");
    setMinimumSize(200, 120);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    statusIndicator_ = new QFrame(this);
    statusIndicator_->setObjectName("NocStatusIndicator");
    statusIndicator_->setFixedSize(16, 16);
    headerLayout->addWidget(statusIndicator_);

    nameLabel_ = new QLabel(QString::fromStdString(host.name), this);
    nameLabel_->setObjectName("NocHostName");
    headerLayout->addWidget(nameLabel_, 1);
    layout->addLayout(headerLayout);

    statusLabel_ = new QLabel("--", this);
    statusLabel_->setObjectName("NocHostStatus");
    layout->addWidget(statusLabel_);

    latencyLabel_ = new QLabel("--", this);
    latencyLabel_->setObjectName("NocHostLatency");
    layout->addWidget(latencyLabel_);

    layout->addStretch();

    updateStatus(host.status, 0.0);
}

void NocHostCard::updateStatus(core::HostStatus status, double latencyMs) {
    QString statusText;
    QString indicatorStyle;

    switch (status) {
    case core::HostStatus::Up:
        statusText = "UP";
        indicatorStyle = "background-color: #27ae60; border-radius: 8px;";
        setProperty("statusClass", "up");
        break;
    case core::HostStatus::Down:
        statusText = "DOWN";
        indicatorStyle = "background-color: #e74c3c; border-radius: 8px;";
        setProperty("statusClass", "down");
        break;
    case core::HostStatus::Warning:
        statusText = "WARNING";
        indicatorStyle = "background-color: #f39c12; border-radius: 8px;";
        setProperty("statusClass", "warning");
        break;
    default:
        statusText = "UNKNOWN";
        indicatorStyle = "background-color: #7f8c8d; border-radius: 8px;";
        setProperty("statusClass", "unknown");
        break;
    }

    statusIndicator_->setStyleSheet(indicatorStyle);
    statusLabel_->setText(statusText);

    if (status == core::HostStatus::Up && latencyMs > 0) {
        latencyLabel_->setText(QString("%1 ms").arg(latencyMs, 0, 'f', 1));
    } else {
        latencyLabel_->setText("--");
    }

    style()->unpolish(this);
    style()->polish(this);
}

} // namespace netpulse::ui
