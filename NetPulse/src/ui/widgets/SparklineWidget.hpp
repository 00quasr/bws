#pragma once

#include "core/types/Host.hpp"

#include <QWidget>
#include <deque>

namespace netpulse::ui {

class SparklineWidget : public QWidget {
    Q_OBJECT

public:
    explicit SparklineWidget(QWidget* parent = nullptr);

    void setMaxDataPoints(int maxPoints);
    int maxDataPoints() const { return maxDataPoints_; }

    void setWarningThreshold(int thresholdMs);
    void setCriticalThreshold(int thresholdMs);

    void addDataPoint(double latencyMs, bool success);
    void setData(const std::deque<double>& latencies, const std::deque<bool>& successes);
    void clear();

    void setHostStatus(core::HostStatus status);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    struct DataPoint {
        double latencyMs{0.0};
        bool success{true};
    };

    QColor lineColor() const;
    QColor fillColor() const;

    std::deque<DataPoint> data_;
    int maxDataPoints_{30};
    int warningThresholdMs_{100};
    int criticalThresholdMs_{500};
    core::HostStatus hostStatus_{core::HostStatus::Unknown};
};

} // namespace netpulse::ui
