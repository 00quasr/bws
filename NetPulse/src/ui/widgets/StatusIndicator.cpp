#include "ui/widgets/StatusIndicator.hpp"

#include <QPainter>

namespace netpulse::ui {

StatusIndicator::StatusIndicator(QWidget* parent) : QWidget(parent) {
    setMinimumSize(16, 16);

    pulseAnimation_ = new QPropertyAnimation(this, "pulseOpacity", this);
    pulseAnimation_->setDuration(1000);
    pulseAnimation_->setStartValue(1.0);
    pulseAnimation_->setEndValue(0.3);
    pulseAnimation_->setEasingCurve(QEasingCurve::InOutSine);
    pulseAnimation_->setLoopCount(-1); // Infinite loop
}

void StatusIndicator::setStatus(core::HostStatus status) {
    if (status_ == status) {
        return;
    }

    status_ = status;

    // Pulse animation for Up status
    if (status == core::HostStatus::Up) {
        startPulseAnimation();
    } else {
        stopPulseAnimation();
    }

    update();
}

void StatusIndicator::setPulseOpacity(qreal opacity) {
    pulseOpacity_ = opacity;
    update();
}

void StatusIndicator::startPulseAnimation() {
    if (pulseAnimation_->state() != QAbstractAnimation::Running) {
        pulseAnimation_->start();
    }
}

void StatusIndicator::stopPulseAnimation() {
    pulseAnimation_->stop();
    pulseOpacity_ = 1.0;
}

QColor StatusIndicator::statusColor() const {
    switch (status_) {
    case core::HostStatus::Up:
        return QColor(0, 200, 0);
    case core::HostStatus::Warning:
        return QColor(255, 165, 0);
    case core::HostStatus::Down:
        return QColor(220, 0, 0);
    case core::HostStatus::Unknown:
    default:
        return QColor(128, 128, 128);
    }
}

void StatusIndicator::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor color = statusColor();
    color.setAlphaF(static_cast<float>(pulseOpacity_));

    // Draw outer glow for Up status
    if (status_ == core::HostStatus::Up) {
        QColor glowColor = color;
        glowColor.setAlphaF(static_cast<float>(0.3 * pulseOpacity_));
        painter.setBrush(glowColor);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(rect().adjusted(0, 0, 0, 0));
    }

    // Draw main circle
    painter.setBrush(color);
    painter.setPen(QPen(color.darker(120), 1));
    int margin = 3;
    painter.drawEllipse(rect().adjusted(margin, margin, -margin, -margin));
}

} // namespace netpulse::ui
