#pragma once

#include "core/types/Host.hpp"
#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTimer>
#include <QPointF>
#include <map>
#include <memory>
#include <vector>

namespace netpulse::ui {

struct TopologyNode {
    int64_t hostId{0};
    QString name;
    QString address;
    core::HostStatus status{core::HostStatus::Unknown};
    QPointF position;
    QPointF velocity{0, 0};
    QGraphicsEllipseItem* graphicsItem{nullptr};
    QGraphicsTextItem* labelItem{nullptr};
    double latencyMs{0.0};
};

struct TopologyEdge {
    int64_t sourceId{0};
    int64_t targetId{0};
    QGraphicsLineItem* graphicsItem{nullptr};
};

class TopologyWidget : public DashboardWidget {
    Q_OBJECT

public:
    explicit TopologyWidget(QWidget* parent = nullptr);

    [[nodiscard]] WidgetType widgetType() const override { return WidgetType::Topology; }

    [[nodiscard]] nlohmann::json settings() const override;
    void applySettings(const nlohmann::json& settings) override;

    void refresh() override;

signals:
    void nodeClicked(int64_t hostId);
    void nodeDoubleClicked(int64_t hostId);

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onLayoutTimer();

private:
    void setupGraphicsView();
    void rebuildTopology();
    void updateNodePositions();
    void updateNodeAppearance();
    void applyForceDirectedLayout();
    void centerGraph();

    QColor statusColor(core::HostStatus status) const;
    void createCentralNode();
    void addHostNode(int64_t hostId, const QString& name, const QString& address,
                     core::HostStatus status, double latencyMs);
    void connectNodes(int64_t sourceId, int64_t targetId);

    QGraphicsView* graphicsView_{nullptr};
    QGraphicsScene* scene_{nullptr};
    QTimer* refreshTimer_{nullptr};
    QTimer* layoutTimer_{nullptr};

    std::map<int64_t, TopologyNode> nodes_;
    std::vector<TopologyEdge> edges_;

    static constexpr int64_t CENTRAL_NODE_ID = -1;
    static constexpr double NODE_RADIUS = 20.0;
    static constexpr double CENTRAL_NODE_RADIUS = 30.0;
    static constexpr double REPULSION_STRENGTH = 5000.0;
    static constexpr double ATTRACTION_STRENGTH = 0.01;
    static constexpr double DAMPING = 0.85;
    static constexpr double MIN_DISTANCE = 80.0;
    static constexpr int LAYOUT_ITERATIONS = 50;

    bool layoutStabilized_{false};
    int layoutIterations_{0};
    bool showLabels_{true};
    bool showLatency_{true};
};

} // namespace netpulse::ui
