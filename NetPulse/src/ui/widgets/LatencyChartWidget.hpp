#pragma once

#include "core/types/PingResult.hpp"

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QWidget>
#include <deque>

namespace netpulse::ui {

class LatencyChartWidget : public QWidget {
    Q_OBJECT

public:
    explicit LatencyChartWidget(QWidget* parent = nullptr);

    void setHost(int64_t hostId);
    void addDataPoint(const core::PingResult& result);
    void clearChart();

    void setMaxDataPoints(int maxPoints) { maxDataPoints_ = maxPoints; }
    int maxDataPoints() const { return maxDataPoints_; }

private:
    void setupChart();
    void updateAxisRanges();

    QChartView* chartView_{nullptr};
    QChart* chart_{nullptr};
    QLineSeries* latencySeries_{nullptr};
    QLineSeries* warningLine_{nullptr};
    QLineSeries* criticalLine_{nullptr};
    QValueAxis* axisX_{nullptr};
    QValueAxis* axisY_{nullptr};

    int64_t hostId_{-1};
    int maxDataPoints_{300};
    std::deque<double> latencyData_;
    int dataIndex_{0};
};

} // namespace netpulse::ui
