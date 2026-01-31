#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QContextMenuEvent>
#include <QHBoxLayout>

namespace netpulse::ui {

DashboardWidget::DashboardWidget(const QString& title, QWidget* parent)
    : QFrame(parent), title_(title) {
    setupBaseUi();
}

void DashboardWidget::setupBaseUi() {
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Raised);
    setStyleSheet(R"(
        DashboardWidget {
            background-color: palette(base);
            border: 1px solid palette(mid);
            border-radius: 6px;
        }
        DashboardWidget:hover {
            border-color: palette(highlight);
        }
    )");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(4);

    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    titleLabel_ = new QLabel(title_, this);
    titleLabel_->setStyleSheet("font-weight: bold; font-size: 12px;");
    headerLayout->addWidget(titleLabel_);

    headerLayout->addStretch();

    menuButton_ = new QPushButton("⋮", this);
    menuButton_->setFixedSize(24, 24);
    menuButton_->setFlat(true);
    menuButton_->setStyleSheet("QPushButton { border: none; font-size: 14px; }");
    connect(menuButton_, &QPushButton::clicked, this, [this]() {
        QMenu menu(this);
        menu.addAction("Settings", this, &DashboardWidget::settingsRequested);
        menu.addSeparator();
        menu.addAction("Remove", this, &DashboardWidget::removeRequested);
        menu.exec(menuButton_->mapToGlobal(QPoint(0, menuButton_->height())));
    });
    headerLayout->addWidget(menuButton_);

    mainLayout->addLayout(headerLayout);

    contentLayout_ = new QVBoxLayout();
    contentLayout_->setContentsMargins(0, 4, 0, 0);
    mainLayout->addLayout(contentLayout_, 1);
}

void DashboardWidget::setTitle(const QString& title) {
    title_ = title;
    titleLabel_->setText(title);
}

void DashboardWidget::setContentWidget(QWidget* content) {
    while (contentLayout_->count() > 0) {
        auto* item = contentLayout_->takeAt(0);
        delete item->widget();
        delete item;
    }
    contentLayout_->addWidget(content);
}

void DashboardWidget::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);
    menu.addAction("Settings", this, &DashboardWidget::settingsRequested);
    menu.addSeparator();
    menu.addAction("Remove", this, &DashboardWidget::removeRequested);
    menu.exec(event->globalPos());
}

QString widgetTypeToString(WidgetType type) {
    switch (type) {
    case WidgetType::Statistics:
        return "Statistics";
    case WidgetType::HostStatus:
        return "HostStatus";
    case WidgetType::Alerts:
        return "Alerts";
    case WidgetType::NetworkOverview:
        return "NetworkOverview";
    case WidgetType::LatencyHistory:
        return "LatencyHistory";
    case WidgetType::Topology:
        return "Topology";
    }
    return "Unknown";
}

WidgetType widgetTypeFromString(const QString& str) {
    if (str == "Statistics")
        return WidgetType::Statistics;
    if (str == "HostStatus")
        return WidgetType::HostStatus;
    if (str == "Alerts")
        return WidgetType::Alerts;
    if (str == "NetworkOverview")
        return WidgetType::NetworkOverview;
    if (str == "LatencyHistory")
        return WidgetType::LatencyHistory;
    if (str == "Topology")
        return WidgetType::Topology;
    return WidgetType::Statistics;
}

nlohmann::json WidgetConfig::toJson() const {
    return {{"type", widgetTypeToString(type).toStdString()},
            {"title", title.toStdString()},
            {"row", row},
            {"col", col},
            {"rowSpan", rowSpan},
            {"colSpan", colSpan},
            {"settings", settings}};
}

WidgetConfig WidgetConfig::fromJson(const nlohmann::json& j) {
    WidgetConfig config;

    if (j.is_null() || !j.is_object()) {
        return config;
    }

    if (j.contains("type") && j["type"].is_string()) {
        config.type = widgetTypeFromString(QString::fromStdString(j["type"].get<std::string>()));
    }
    if (j.contains("title") && j["title"].is_string()) {
        config.title = QString::fromStdString(j["title"].get<std::string>());
    }
    if (j.contains("row") && j["row"].is_number()) {
        config.row = j["row"].get<int>();
    }
    if (j.contains("col") && j["col"].is_number()) {
        config.col = j["col"].get<int>();
    }
    if (j.contains("rowSpan") && j["rowSpan"].is_number()) {
        config.rowSpan = j["rowSpan"].get<int>();
    }
    if (j.contains("colSpan") && j["colSpan"].is_number()) {
        config.colSpan = j["colSpan"].get<int>();
    }
    if (j.contains("settings") && j["settings"].is_object()) {
        config.settings = j["settings"];
    }

    return config;
}

} // namespace netpulse::ui
