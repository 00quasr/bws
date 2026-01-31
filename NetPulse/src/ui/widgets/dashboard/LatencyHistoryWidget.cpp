#include "ui/widgets/dashboard/LatencyHistoryWidget.hpp"

#include "app/Application.hpp"

#include <QVBoxLayout>

namespace netpulse::ui {

LatencyHistoryWidget::LatencyHistoryWidget(QWidget* parent)
    : DashboardWidget("Latency History", parent) {
    auto* chart = new QChart();
    chart->legend()->hide();
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->setBackgroundVisible(false);

    latencySeries_ = new QLineSeries(this);
    latencySeries_->setColor(QColor("#3498db"));
    chart->addSeries(latencySeries_);

    axisX_ = new QValueAxis(this);
    axisX_->setRange(0, maxDataPoints_);
    axisX_->setLabelsVisible(false);
    axisX_->setGridLineVisible(false);
    chart->addAxis(axisX_, Qt::AlignBottom);
    latencySeries_->attachAxis(axisX_);

    axisY_ = new QValueAxis(this);
    axisY_->setRange(0, 100);
    axisY_->setTitleText("ms");
    axisY_->setGridLineColor(QColor(128, 128, 128, 50));
    chart->addAxis(axisY_, Qt::AlignLeft);
    latencySeries_->attachAxis(axisY_);

    chartView_ = new QChartView(chart, this);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartView_->setStyleSheet("background: transparent;");

    setContentWidget(chartView_);

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &LatencyHistoryWidget::refresh);
    refreshTimer_->start(1000);

    auto& dashVm = app::Application::instance().dashboardViewModel();
    connect(&dashVm, &viewmodels::DashboardViewModel::pingResultReceived, this,
            [this](int64_t hostId, const core::PingResult& result) {
                if (hostId == hostId_ && result.success) {
                    double latency = result.latencyMs();

                    latencySeries_->append(dataPointCount_, latency);
                    dataPointCount_++;

                    if (latencySeries_->count() > maxDataPoints_) {
                        latencySeries_->remove(0);
                        axisX_->setRange(dataPointCount_ - maxDataPoints_,
                                         dataPointCount_);
                    }

                    double maxY = 100;
                    for (int i = 0; i < latencySeries_->count(); ++i) {
                        maxY = std::max(maxY, latencySeries_->at(i).y() * 1.2);
                    }
                    axisY_->setRange(0, maxY);
                }
            });
}

nlohmann::json LatencyHistoryWidget::settings() const {
    return {{"hostId", hostId_}, {"maxDataPoints", maxDataPoints_}};
}

void LatencyHistoryWidget::applySettings(const nlohmann::json& settings) {
    hostId_ = settings.value("hostId", -1);
    maxDataPoints_ = settings.value("maxDataPoints", 60);
    axisX_->setRange(0, maxDataPoints_);
    refresh();
}

void LatencyHistoryWidget::setHostId(int64_t hostId) {
    hostId_ = hostId;
    latencySeries_->clear();
    dataPointCount_ = 0;

    if (hostId >= 0) {
        auto& vm = app::Application::instance().hostMonitorViewModel();
        auto host = vm.getHost(hostId);
        if (host) {
            setTitle(QString("Latency: %1").arg(QString::fromStdString(host->name)));
        }
    } else {
        setTitle("Latency History");
    }

    refresh();
}

void LatencyHistoryWidget::refresh() {
    if (hostId_ < 0)
        return;

    auto& vm = app::Application::instance().dashboardViewModel();
    auto results = vm.getRecentResults(hostId_, maxDataPoints_);

    latencySeries_->clear();
    dataPointCount_ = 0;

    double maxY = 100;
    for (auto it = results.rbegin(); it != results.rend(); ++it) {
        if (it->success) {
            double latency = it->latencyMs();
            latencySeries_->append(dataPointCount_, latency);
            maxY = std::max(maxY, latency * 1.2);
        }
        dataPointCount_++;
    }

    axisX_->setRange(0, std::max(maxDataPoints_, dataPointCount_));
    axisY_->setRange(0, maxY);
}

} // namespace netpulse::ui
