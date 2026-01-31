#include "ui/widgets/SparklineWidget.hpp"

#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace netpulse::ui {

namespace {
constexpr int SPARKLINE_WIDTH = 80;
constexpr int SPARKLINE_HEIGHT = 24;
constexpr int PADDING = 2;

// Colors
const QColor COLOR_UP(76, 175, 80);        // Green
const QColor COLOR_WARNING(255, 152, 0);   // Orange
const QColor COLOR_DOWN(244, 67, 54);      // Red
const QColor COLOR_UNKNOWN(158, 158, 158); // Gray
} // namespace

SparklineWidget::SparklineWidget(QWidget* parent) : QWidget(parent) {
    setFixedSize(SPARKLINE_WIDTH, SPARKLINE_HEIGHT);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void SparklineWidget::setMaxDataPoints(int maxPoints) {
    maxDataPoints_ = std::max(2, maxPoints);
    while (static_cast<int>(data_.size()) > maxDataPoints_) {
        data_.pop_front();
    }
    update();
}

void SparklineWidget::setWarningThreshold(int thresholdMs) {
    warningThresholdMs_ = thresholdMs;
    update();
}

void SparklineWidget::setCriticalThreshold(int thresholdMs) {
    criticalThresholdMs_ = thresholdMs;
    update();
}

void SparklineWidget::addDataPoint(double latencyMs, bool success) {
    data_.push_back({latencyMs, success});
    while (static_cast<int>(data_.size()) > maxDataPoints_) {
        data_.pop_front();
    }
    update();
}

void SparklineWidget::setData(const std::deque<double>& latencies,
                               const std::deque<bool>& successes) {
    data_.clear();
    size_t count = std::min(latencies.size(), successes.size());
    for (size_t i = 0; i < count; ++i) {
        data_.push_back({latencies[i], successes[i]});
    }
    while (static_cast<int>(data_.size()) > maxDataPoints_) {
        data_.pop_front();
    }
    update();
}

void SparklineWidget::clear() {
    data_.clear();
    update();
}

void SparklineWidget::setHostStatus(core::HostStatus status) {
    hostStatus_ = status;
    update();
}

QSize SparklineWidget::sizeHint() const {
    return {SPARKLINE_WIDTH, SPARKLINE_HEIGHT};
}

QSize SparklineWidget::minimumSizeHint() const {
    return {SPARKLINE_WIDTH, SPARKLINE_HEIGHT};
}

QColor SparklineWidget::lineColor() const {
    switch (hostStatus_) {
    case core::HostStatus::Up:
        return COLOR_UP;
    case core::HostStatus::Warning:
        return COLOR_WARNING;
    case core::HostStatus::Down:
        return COLOR_DOWN;
    default:
        return COLOR_UNKNOWN;
    }
}

QColor SparklineWidget::fillColor() const {
    QColor color = lineColor();
    color.setAlpha(40);
    return color;
}

void SparklineWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int w = width() - 2 * PADDING;
    const int h = height() - 2 * PADDING;

    if (data_.size() < 2) {
        // Draw a placeholder line when no data
        painter.setPen(QPen(COLOR_UNKNOWN, 1));
        painter.drawLine(PADDING, height() / 2, width() - PADDING, height() / 2);
        return;
    }

    // Find min/max for scaling
    double minLatency = std::numeric_limits<double>::max();
    double maxLatency = 0.0;
    for (const auto& point : data_) {
        if (point.success) {
            minLatency = std::min(minLatency, point.latencyMs);
            maxLatency = std::max(maxLatency, point.latencyMs);
        }
    }

    // Ensure we have a reasonable range
    if (maxLatency <= minLatency) {
        maxLatency = minLatency + 10.0;
    }
    // Ensure minimum range for visualization
    double range = maxLatency - minLatency;
    if (range < 5.0) {
        double mid = (maxLatency + minLatency) / 2.0;
        minLatency = mid - 2.5;
        maxLatency = mid + 2.5;
        range = 5.0;
    }

    // Build the sparkline path
    QPainterPath linePath;
    QPainterPath fillPath;
    bool firstPoint = true;
    double xStep = static_cast<double>(w) / static_cast<double>(data_.size() - 1);

    for (size_t i = 0; i < data_.size(); ++i) {
        const auto& point = data_[i];
        double x = PADDING + static_cast<double>(i) * xStep;

        double normalizedY;
        if (point.success) {
            normalizedY = 1.0 - ((point.latencyMs - minLatency) / range);
        } else {
            // Failed pings shown at top (high latency)
            normalizedY = 0.0;
        }

        double y = PADDING + (1.0 - normalizedY) * h;
        // Clamp y to valid range
        y = std::clamp(y, static_cast<double>(PADDING), static_cast<double>(height() - PADDING));

        if (firstPoint) {
            linePath.moveTo(x, y);
            fillPath.moveTo(x, height() - PADDING);
            fillPath.lineTo(x, y);
            firstPoint = false;
        } else {
            linePath.lineTo(x, y);
            fillPath.lineTo(x, y);
        }
    }

    // Close the fill path
    fillPath.lineTo(width() - PADDING, height() - PADDING);
    fillPath.closeSubpath();

    // Draw fill
    painter.fillPath(fillPath, fillColor());

    // Draw threshold lines (subtle dashed)
    QPen thresholdPen(COLOR_WARNING.lighter(150), 1, Qt::DotLine);
    painter.setPen(thresholdPen);
    double warningY = PADDING + (1.0 - ((warningThresholdMs_ - minLatency) / range)) * h;
    if (warningY > PADDING && warningY < height() - PADDING) {
        painter.drawLine(PADDING, static_cast<int>(warningY), width() - PADDING,
                         static_cast<int>(warningY));
    }

    // Draw main line
    QPen linePen(lineColor(), 1.5);
    linePen.setCapStyle(Qt::RoundCap);
    linePen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(linePen);
    painter.drawPath(linePath);

    // Draw failure markers (small red dots for failed pings)
    painter.setPen(Qt::NoPen);
    painter.setBrush(COLOR_DOWN);
    for (size_t i = 0; i < data_.size(); ++i) {
        if (!data_[i].success) {
            double x = PADDING + static_cast<double>(i) * xStep;
            painter.drawEllipse(QPointF(x, PADDING + 2), 2, 2);
        }
    }
}

} // namespace netpulse::ui
