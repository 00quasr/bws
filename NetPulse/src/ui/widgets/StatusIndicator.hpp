#pragma once

#include "core/types/Host.hpp"

#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

namespace netpulse::ui {

class StatusIndicator : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal pulseOpacity READ pulseOpacity WRITE setPulseOpacity)

public:
    explicit StatusIndicator(QWidget* parent = nullptr);

    void setStatus(core::HostStatus status);
    core::HostStatus status() const { return status_; }

    qreal pulseOpacity() const { return pulseOpacity_; }
    void setPulseOpacity(qreal opacity);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void startPulseAnimation();
    void stopPulseAnimation();
    QColor statusColor() const;

    core::HostStatus status_{core::HostStatus::Unknown};
    qreal pulseOpacity_{1.0};
    QPropertyAnimation* pulseAnimation_{nullptr};
};

} // namespace netpulse::ui
