#include "ui/resources/AppIcon.hpp"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

namespace netpulse::ui {

namespace {
// Brand colors
const QColor kPrimaryBlue("#007acc");
const QColor kDarkerBlue("#005a9e");
const QColor kCyan("#00d4ff");
const QColor kWhite(Qt::white);
} // namespace

QIcon AppIcon::applicationIcon() {
    QIcon icon;

    // Add multiple sizes for different use cases
    const int sizes[] = {16, 22, 24, 32, 48, 64, 128, 256};
    for (int size : sizes) {
        // Use simplified version for very small sizes
        bool simplified = (size <= 24);
        icon.addPixmap(createPixmap(size, simplified));
    }

    return icon;
}

QIcon AppIcon::trayIcon() {
    QIcon icon;

    // System tray typically uses smaller sizes
    const int sizes[] = {16, 22, 24, 32, 48};
    for (int size : sizes) {
        icon.addPixmap(createPixmap(size, true));
    }

    return icon;
}

QPixmap AppIcon::createPixmap(int size, bool simplified) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    drawIcon(painter, size, simplified);

    painter.end();
    return pixmap;
}

void AppIcon::drawIcon(QPainter& painter, int size, bool simplified) {
    const qreal scale = size / 64.0;
    painter.scale(scale, scale);

    // Background with gradient
    QLinearGradient bgGrad(0, 0, 64, 64);
    bgGrad.setColorAt(0, kDarkerBlue);
    bgGrad.setColorAt(1, kPrimaryBlue);

    if (simplified) {
        // Rounded rectangle for small sizes
        painter.setPen(Qt::NoPen);
        painter.setBrush(bgGrad);
        painter.drawRoundedRect(4, 4, 56, 56, 12, 12);
    } else {
        // Circle for larger sizes
        painter.setPen(Qt::NoPen);
        painter.setBrush(bgGrad);
        painter.drawEllipse(2, 2, 60, 60);
    }

    // Draw the pulse line
    drawPulseLine(painter, 64, simplified);

    if (!simplified) {
        // Add network nodes for larger sizes
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 180));

        // Left and right nodes
        painter.drawEllipse(12, 28, 8, 8);
        painter.drawEllipse(44, 28, 8, 8);

        // Top and bottom nodes (smaller)
        painter.drawEllipse(29, 12, 6, 6);
        painter.drawEllipse(29, 46, 6, 6);

        // Connection lines (faded)
        painter.setPen(QPen(QColor(255, 255, 255, 80), 1.5));
        painter.drawLine(20, 32, 26, 32);
        painter.drawLine(38, 32, 44, 32);
        painter.drawLine(32, 18, 32, 24);
        painter.drawLine(32, 40, 32, 46);

        // Center highlight
        painter.setPen(Qt::NoPen);
        painter.setBrush(kCyan);
        painter.drawEllipse(26, 26, 12, 12);
        painter.setBrush(kWhite);
        painter.drawEllipse(28, 28, 8, 8);
    }
}

void AppIcon::drawPulseLine(QPainter& painter, int /*size*/, bool simplified) {
    // Create pulse/heartbeat polyline
    QPolygonF pulse;

    if (simplified) {
        // Simpler pulse for small sizes
        pulse << QPointF(10, 32) << QPointF(20, 32) << QPointF(26, 22) << QPointF(32, 42)
              << QPointF(38, 18) << QPointF(44, 38) << QPointF(50, 32) << QPointF(54, 32);
    } else {
        // More detailed pulse for larger sizes
        pulse << QPointF(8, 32) << QPointF(16, 32) << QPointF(22, 32) << QPointF(26, 22)
              << QPointF(30, 42) << QPointF(34, 18) << QPointF(38, 40) << QPointF(42, 32)
              << QPointF(48, 32) << QPointF(56, 32);
    }

    // Create gradient for pulse line
    QLinearGradient pulseGrad(8, 0, 56, 0);
    pulseGrad.setColorAt(0, kCyan);
    pulseGrad.setColorAt(0.5, kWhite);
    pulseGrad.setColorAt(1, kCyan);

    // Draw the pulse
    QPen pulsePen(QBrush(pulseGrad), simplified ? 4.0 : 3.0);
    pulsePen.setCapStyle(Qt::RoundCap);
    pulsePen.setJoinStyle(Qt::RoundJoin);

    painter.setPen(pulsePen);
    painter.setBrush(Qt::NoBrush);

    QPainterPath path;
    path.addPolygon(pulse);
    painter.drawPath(path);
}

} // namespace netpulse::ui
