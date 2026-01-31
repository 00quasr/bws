#include "ui/widgets/LatencyChartWidget.hpp"

#include "app/Application.hpp"

#include <QVBoxLayout>

namespace netpulse::ui {

LatencyChartWidget::LatencyChartWidget(QWidget* parent) : QWidget(parent) {
    setupChart();
}

void LatencyChartWidget::setupChart() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    chart_ = new QChart();
    chart_->setTitle("Latency (ms)");
    chart_->setAnimationOptions(QChart::NoAnimation);
    chart_->legend()->hide();

    // Main latency series
    latencySeries_ = new QLineSeries(this);
    latencySeries_->setName("Latency");
    latencySeries_->setColor(QColor(0, 150, 255));
    chart_->addSeries(latencySeries_);

    // Warning threshold line
    warningLine_ = new QLineSeries(this);
    warningLine_->setName("Warning");
    warningLine_->setColor(QColor(255, 165, 0));
    QPen warningPen = warningLine_->pen();
    warningPen.setStyle(Qt::DashLine);
    warningLine_->setPen(warningPen);
    chart_->addSeries(warningLine_);

    // Critical threshold line
    criticalLine_ = new QLineSeries(this);
    criticalLine_->setName("Critical");
    criticalLine_->setColor(QColor(255, 0, 0));
    QPen criticalPen = criticalLine_->pen();
    criticalPen.setStyle(Qt::DashLine);
    criticalLine_->setPen(criticalPen);
    chart_->addSeries(criticalLine_);

    // X axis (time/samples)
    axisX_ = new QValueAxis(this);
    axisX_->setTitleText("Samples");
    axisX_->setLabelFormat("%d");
    axisX_->setRange(0, maxDataPoints_);
    chart_->addAxis(axisX_, Qt::AlignBottom);
    latencySeries_->attachAxis(axisX_);
    warningLine_->attachAxis(axisX_);
    criticalLine_->attachAxis(axisX_);

    // Y axis (latency in ms)
    axisY_ = new QValueAxis(this);
    axisY_->setTitleText("ms");
    axisY_->setLabelFormat("%.1f");
    axisY_->setRange(0, 100);
    chart_->addAxis(axisY_, Qt::AlignLeft);
    latencySeries_->attachAxis(axisY_);
    warningLine_->attachAxis(axisY_);
    criticalLine_->attachAxis(axisY_);

    chartView_ = new QChartView(chart_, this);
    chartView_->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(chartView_);
}

void LatencyChartWidget::setHost(int64_t hostId) {
    hostId_ = hostId;
    clearChart();

    // Update threshold lines based on host settings
    auto& vm = app::Application::instance().hostMonitorViewModel();
    auto host = vm.getHost(hostId);

    if (host) {
        double warning = static_cast<double>(host->warningThresholdMs);
        double critical = static_cast<double>(host->criticalThresholdMs);

        warningLine_->clear();
        warningLine_->append(0, warning);
        warningLine_->append(maxDataPoints_, warning);

        criticalLine_->clear();
        criticalLine_->append(0, critical);
        criticalLine_->append(maxDataPoints_, critical);

        chart_->setTitle(QString("Latency - %1").arg(QString::fromStdString(host->name)));
    }
}

void LatencyChartWidget::addDataPoint(const core::PingResult& result) {
    double latency = result.success ? result.latencyMs() : 0.0;

    latencyData_.push_back(latency);
    if (static_cast<int>(latencyData_.size()) > maxDataPoints_) {
        latencyData_.pop_front();
    }

    // Rebuild series from data
    latencySeries_->clear();
    int idx = 0;
    for (double lat : latencyData_) {
        latencySeries_->append(idx++, lat);
    }

    updateAxisRanges();
}

void LatencyChartWidget::clearChart() {
    latencyData_.clear();
    latencySeries_->clear();
    dataIndex_ = 0;
    axisY_->setRange(0, 100);
}

void LatencyChartWidget::updateAxisRanges() {
    if (latencyData_.empty()) {
        return;
    }

    // Update X axis
    int count = static_cast<int>(latencyData_.size());
    axisX_->setRange(0, std::max(count, maxDataPoints_));

    // Update Y axis based on max latency
    double maxLatency = *std::max_element(latencyData_.begin(), latencyData_.end());

    // Get thresholds for comparison
    auto& vm = app::Application::instance().hostMonitorViewModel();
    auto host = vm.getHost(hostId_);
    if (host) {
        maxLatency = std::max(maxLatency, static_cast<double>(host->criticalThresholdMs));
    }

    // Add some headroom
    double yMax = std::max(100.0, maxLatency * 1.2);
    axisY_->setRange(0, yMax);
}

} // namespace netpulse::ui
