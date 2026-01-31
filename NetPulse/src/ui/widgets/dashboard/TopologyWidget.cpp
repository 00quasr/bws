#include "ui/widgets/dashboard/TopologyWidget.hpp"

#include "app/Application.hpp"
#include "core/types/Host.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QResizeEvent>
#include <QVBoxLayout>
#include <cmath>
#include <random>
#include <set>

namespace netpulse::ui {

TopologyWidget::TopologyWidget(QWidget* parent)
    : DashboardWidget("Network Topology", parent) {
    setupGraphicsView();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &TopologyWidget::refresh);
    refreshTimer_->start(3000);

    layoutTimer_ = new QTimer(this);
    connect(layoutTimer_, &QTimer::timeout, this, &TopologyWidget::onLayoutTimer);

    refresh();
}

void TopologyWidget::setupGraphicsView() {
    auto* contentWidget = new QWidget(this);
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(0, 0, 0, 0);

    scene_ = new QGraphicsScene(this);
    scene_->setBackgroundBrush(QBrush(QColor("#2c3e50")));

    graphicsView_ = new QGraphicsView(scene_, this);
    graphicsView_->setRenderHint(QPainter::Antialiasing);
    graphicsView_->setRenderHint(QPainter::SmoothPixmapTransform);
    graphicsView_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    graphicsView_->setStyleSheet(R"(
        QGraphicsView {
            border: none;
            background: #2c3e50;
        }
    )");

    layout->addWidget(graphicsView_);
    setContentWidget(contentWidget);
}

nlohmann::json TopologyWidget::settings() const {
    return {{"showLabels", showLabels_}, {"showLatency", showLatency_}};
}

void TopologyWidget::applySettings(const nlohmann::json& settings) {
    showLabels_ = settings.value("showLabels", true);
    showLatency_ = settings.value("showLatency", true);
    rebuildTopology();
}

void TopologyWidget::refresh() {
    auto& vm = app::Application::instance().dashboardViewModel();
    auto hosts = vm.getHosts();

    bool needsRebuild = false;

    std::set<int64_t> currentHostIds;
    for (const auto& host : hosts) {
        if (!host.enabled)
            continue;
        currentHostIds.insert(host.id);

        auto it = nodes_.find(host.id);
        if (it == nodes_.end()) {
            needsRebuild = true;
        } else {
            auto stats = vm.getStatistics(host.id);
            it->second.status = host.status;
            it->second.latencyMs = static_cast<double>(stats.avgLatency.count()) / 1000.0;
        }
    }

    for (const auto& [id, node] : nodes_) {
        if (id != CENTRAL_NODE_ID && currentHostIds.find(id) == currentHostIds.end()) {
            needsRebuild = true;
            break;
        }
    }

    if (needsRebuild) {
        rebuildTopology();
    } else {
        updateNodeAppearance();
    }
}

void TopologyWidget::rebuildTopology() {
    scene_->clear();
    nodes_.clear();
    edges_.clear();
    layoutStabilized_ = false;
    layoutIterations_ = 0;

    createCentralNode();

    auto& vm = app::Application::instance().dashboardViewModel();
    auto hosts = vm.getHosts();

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(-100.0, 100.0);

    for (const auto& host : hosts) {
        if (!host.enabled)
            continue;

        auto stats = vm.getStatistics(host.id);
        addHostNode(host.id, QString::fromStdString(host.name),
                    QString::fromStdString(host.address), host.status,
                    static_cast<double>(stats.avgLatency.count()) / 1000.0);

        nodes_[host.id].position = QPointF(dis(gen), dis(gen));

        connectNodes(CENTRAL_NODE_ID, host.id);
    }

    updateNodePositions();
    updateNodeAppearance();

    layoutTimer_->start(16);
}

void TopologyWidget::createCentralNode() {
    TopologyNode central;
    central.hostId = CENTRAL_NODE_ID;
    central.name = "NetPulse";
    central.address = "Monitor";
    central.status = core::HostStatus::Up;
    central.position = QPointF(0, 0);

    auto* ellipse = scene_->addEllipse(-CENTRAL_NODE_RADIUS, -CENTRAL_NODE_RADIUS,
                                        CENTRAL_NODE_RADIUS * 2, CENTRAL_NODE_RADIUS * 2,
                                        QPen(Qt::white, 2), QBrush(QColor("#3498db")));
    ellipse->setZValue(10);
    central.graphicsItem = ellipse;

    if (showLabels_) {
        auto* label = scene_->addText(central.name, QFont("Arial", 8, QFont::Bold));
        label->setDefaultTextColor(Qt::white);
        label->setZValue(11);
        central.labelItem = label;
    }

    nodes_[CENTRAL_NODE_ID] = central;
}

void TopologyWidget::addHostNode(int64_t hostId, const QString& name, const QString& address,
                                  core::HostStatus status, double latencyMs) {
    TopologyNode node;
    node.hostId = hostId;
    node.name = name;
    node.address = address;
    node.status = status;
    node.latencyMs = latencyMs;
    node.position = QPointF(0, 0);

    auto* ellipse =
        scene_->addEllipse(-NODE_RADIUS, -NODE_RADIUS, NODE_RADIUS * 2, NODE_RADIUS * 2,
                           QPen(Qt::white, 1.5), QBrush(statusColor(status)));
    ellipse->setData(0, QVariant::fromValue(hostId));
    ellipse->setFlag(QGraphicsItem::ItemIsSelectable);
    ellipse->setZValue(5);
    ellipse->setCursor(Qt::PointingHandCursor);
    node.graphicsItem = ellipse;

    if (showLabels_) {
        QString labelText = name;
        if (showLatency_ && latencyMs > 0) {
            labelText += QString("\n%1ms").arg(latencyMs, 0, 'f', 1);
        }
        auto* label = scene_->addText(labelText, QFont("Arial", 7));
        label->setDefaultTextColor(Qt::white);
        label->setZValue(6);
        node.labelItem = label;
    }

    nodes_[hostId] = node;
}

void TopologyWidget::connectNodes(int64_t sourceId, int64_t targetId) {
    auto* line = scene_->addLine(0, 0, 0, 0, QPen(QColor("#7f8c8d"), 1.5));
    line->setZValue(1);

    TopologyEdge edge;
    edge.sourceId = sourceId;
    edge.targetId = targetId;
    edge.graphicsItem = line;

    edges_.push_back(edge);
}

void TopologyWidget::onLayoutTimer() {
    if (layoutStabilized_) {
        layoutTimer_->stop();
        return;
    }

    applyForceDirectedLayout();
    updateNodePositions();
    layoutIterations_++;

    if (layoutIterations_ >= LAYOUT_ITERATIONS) {
        layoutStabilized_ = true;
        layoutTimer_->stop();
        centerGraph();
    }
}

void TopologyWidget::applyForceDirectedLayout() {
    if (nodes_.size() <= 1)
        return;

    for (auto& [id, node] : nodes_) {
        if (id == CENTRAL_NODE_ID)
            continue;

        QPointF force(0, 0);

        for (const auto& [otherId, other] : nodes_) {
            if (id == otherId)
                continue;

            QPointF delta = node.position - other.position;
            double distance = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());
            if (distance < 1.0)
                distance = 1.0;

            double repulsion = REPULSION_STRENGTH / (distance * distance);
            force += delta / distance * repulsion;
        }

        for (const auto& edge : edges_) {
            int64_t connectedId = -1;
            if (edge.sourceId == id)
                connectedId = edge.targetId;
            else if (edge.targetId == id)
                connectedId = edge.sourceId;

            if (connectedId != -1) {
                auto it = nodes_.find(connectedId);
                if (it != nodes_.end()) {
                    QPointF delta = it->second.position - node.position;
                    double distance = std::sqrt(delta.x() * delta.x() + delta.y() * delta.y());

                    double attraction = distance * ATTRACTION_STRENGTH;
                    if (distance > MIN_DISTANCE) {
                        force += delta / distance * attraction;
                    }
                }
            }
        }

        node.velocity = (node.velocity + force) * DAMPING;

        double maxVelocity = 50.0;
        double velocityMagnitude =
            std::sqrt(node.velocity.x() * node.velocity.x() + node.velocity.y() * node.velocity.y());
        if (velocityMagnitude > maxVelocity) {
            node.velocity = node.velocity / velocityMagnitude * maxVelocity;
        }

        node.position += node.velocity;
    }
}

void TopologyWidget::updateNodePositions() {
    for (auto& [id, node] : nodes_) {
        if (node.graphicsItem) {
            node.graphicsItem->setPos(node.position);
        }
        if (node.labelItem) {
            double radius = (id == CENTRAL_NODE_ID) ? CENTRAL_NODE_RADIUS : NODE_RADIUS;
            node.labelItem->setPos(node.position.x() - node.labelItem->boundingRect().width() / 2,
                                   node.position.y() + radius + 2);
        }
    }

    for (auto& edge : edges_) {
        auto sourceIt = nodes_.find(edge.sourceId);
        auto targetIt = nodes_.find(edge.targetId);

        if (sourceIt != nodes_.end() && targetIt != nodes_.end() && edge.graphicsItem) {
            edge.graphicsItem->setLine(
                QLineF(sourceIt->second.position, targetIt->second.position));

            auto targetStatus = targetIt->second.status;
            QColor lineColor;
            switch (targetStatus) {
            case core::HostStatus::Up:
                lineColor = QColor("#27ae60");
                break;
            case core::HostStatus::Warning:
                lineColor = QColor("#e67e22");
                break;
            case core::HostStatus::Down:
                lineColor = QColor("#e74c3c");
                break;
            default:
                lineColor = QColor("#7f8c8d");
                break;
            }
            edge.graphicsItem->setPen(QPen(lineColor, 1.5));
        }
    }
}

void TopologyWidget::updateNodeAppearance() {
    for (auto& [id, node] : nodes_) {
        if (id == CENTRAL_NODE_ID)
            continue;

        if (node.graphicsItem) {
            node.graphicsItem->setBrush(QBrush(statusColor(node.status)));
        }

        if (node.labelItem && showLatency_) {
            QString labelText = node.name;
            if (node.latencyMs > 0) {
                labelText += QString("\n%1ms").arg(node.latencyMs, 0, 'f', 1);
            }
            node.labelItem->setPlainText(labelText);
        }
    }

    for (auto& edge : edges_) {
        auto targetIt = nodes_.find(edge.targetId);
        if (targetIt != nodes_.end() && edge.graphicsItem) {
            auto targetStatus = targetIt->second.status;
            QColor lineColor;
            switch (targetStatus) {
            case core::HostStatus::Up:
                lineColor = QColor("#27ae60");
                break;
            case core::HostStatus::Warning:
                lineColor = QColor("#e67e22");
                break;
            case core::HostStatus::Down:
                lineColor = QColor("#e74c3c");
                break;
            default:
                lineColor = QColor("#7f8c8d");
                break;
            }
            edge.graphicsItem->setPen(QPen(lineColor, 1.5));
        }
    }
}

void TopologyWidget::centerGraph() {
    if (nodes_.empty())
        return;

    graphicsView_->fitInView(scene_->itemsBoundingRect().adjusted(-50, -50, 50, 50),
                              Qt::KeepAspectRatio);
}

void TopologyWidget::resizeEvent(QResizeEvent* event) {
    DashboardWidget::resizeEvent(event);
    if (layoutStabilized_) {
        centerGraph();
    }
}

QColor TopologyWidget::statusColor(core::HostStatus status) const {
    switch (status) {
    case core::HostStatus::Up:
        return QColor("#27ae60");
    case core::HostStatus::Warning:
        return QColor("#e67e22");
    case core::HostStatus::Down:
        return QColor("#e74c3c");
    default:
        return QColor("#95a5a6");
    }
}

} // namespace netpulse::ui
