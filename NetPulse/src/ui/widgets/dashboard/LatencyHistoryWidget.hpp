#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QTimer>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

namespace netpulse::ui {

class LatencyHistoryWidget : public DashboardWidget {
    Q_OBJECT

public:
    explicit LatencyHistoryWidget(QWidget* parent = nullptr);

    [[nodiscard]] WidgetType widgetType() const override { return WidgetType::LatencyHistory; }

    [[nodiscard]] nlohmann::json settings() const override;
    void applySettings(const nlohmann::json& settings) override;

    void setHostId(int64_t hostId);
    void refresh() override;

private:
    QChartView* chartView_{nullptr};
    QLineSeries* latencySeries_{nullptr};
    QValueAxis* axisX_{nullptr};
    QValueAxis* axisY_{nullptr};

    QTimer* refreshTimer_{nullptr};

    int64_t hostId_{-1};
    int maxDataPoints_{60};
    int dataPointCount_{0};
};

} // namespace netpulse::ui
