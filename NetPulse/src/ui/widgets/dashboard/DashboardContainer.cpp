#include "ui/widgets/dashboard/DashboardContainer.hpp"

#include <QInputDialog>
#include <QMessageBox>
#include <QVBoxLayout>

namespace netpulse::ui {

DashboardContainer::DashboardContainer(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void DashboardContainer::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    scrollArea_ = new QScrollArea(this);
    scrollArea_->setWidgetResizable(true);
    scrollArea_->setFrameShape(QFrame::NoFrame);

    containerWidget_ = new QWidget(scrollArea_);
    gridLayout_ = new QGridLayout(containerWidget_);
    gridLayout_->setContentsMargins(8, 8, 8, 8);
    gridLayout_->setSpacing(12);

    for (int i = 0; i < 3; ++i) {
        gridLayout_->setColumnStretch(i, 1);
    }

    scrollArea_->setWidget(containerWidget_);
    mainLayout->addWidget(scrollArea_);
}

void DashboardContainer::addWidget(WidgetType type, int row, int col, int rowSpan, int colSpan) {
    WidgetConfig config;
    config.type = type;
    config.title = widgetTypeToString(type);
    config.row = row;
    config.col = col;
    config.rowSpan = rowSpan;
    config.colSpan = colSpan;

    addWidget(config);
}

void DashboardContainer::addWidget(const WidgetConfig& config) {
    auto widget = WidgetFactory::createFromConfig(config, containerWidget_);

    connect(widget.get(), &DashboardWidget::removeRequested, this,
            &DashboardContainer::onWidgetRemoveRequested);
    connect(widget.get(), &DashboardWidget::settingsRequested, this,
            &DashboardContainer::onWidgetSettingsRequested);

    widget->setMinimumSize(200, 150);

    gridLayout_->addWidget(widget.get(), config.row, config.col, config.rowSpan, config.colSpan);

    widgets_.push_back(
        {std::move(widget), config.row, config.col, config.rowSpan, config.colSpan});

    emit layoutChanged();
}

void DashboardContainer::removeWidget(DashboardWidget* widget) {
    auto it = std::find_if(widgets_.begin(), widgets_.end(),
                           [widget](const WidgetEntry& entry) {
                               return entry.widget.get() == widget;
                           });

    if (it != widgets_.end()) {
        gridLayout_->removeWidget(it->widget.get());
        widgets_.erase(it);
        emit widgetRemoved();
        emit layoutChanged();
    }
}

void DashboardContainer::clearWidgets() {
    for (auto& entry : widgets_) {
        gridLayout_->removeWidget(entry.widget.get());
    }
    widgets_.clear();
    emit layoutChanged();
}

std::vector<WidgetConfig> DashboardContainer::getConfiguration() const {
    std::vector<WidgetConfig> configs;
    configs.reserve(widgets_.size());

    for (const auto& entry : widgets_) {
        WidgetConfig config;
        config.type = entry.widget->widgetType();
        config.title = entry.widget->title();
        config.row = entry.row;
        config.col = entry.col;
        config.rowSpan = entry.rowSpan;
        config.colSpan = entry.colSpan;
        config.settings = entry.widget->settings();
        configs.push_back(config);
    }

    return configs;
}

void DashboardContainer::loadConfiguration(const std::vector<WidgetConfig>& configs) {
    clearWidgets();
    for (const auto& config : configs) {
        addWidget(config);
    }
}

void DashboardContainer::loadDefaultLayout() {
    clearWidgets();

    addWidget(WidgetType::Statistics, 0, 0, 1, 1);
    addWidget(WidgetType::HostStatus, 0, 1, 1, 1);
    addWidget(WidgetType::Alerts, 0, 2, 1, 1);
    addWidget(WidgetType::NetworkOverview, 1, 0, 1, 1);
    addWidget(WidgetType::LatencyHistory, 1, 1, 1, 2);
}

void DashboardContainer::onWidgetRemoveRequested() {
    auto* widget = qobject_cast<DashboardWidget*>(sender());
    if (!widget)
        return;

    auto reply = QMessageBox::question(
        this, "Remove Widget",
        QString("Are you sure you want to remove '%1'?").arg(widget->title()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        removeWidget(widget);
    }
}

void DashboardContainer::onWidgetSettingsRequested() {
    auto* widget = qobject_cast<DashboardWidget*>(sender());
    if (!widget)
        return;

    bool ok;
    QString newTitle = QInputDialog::getText(
        this, "Widget Settings", "Title:", QLineEdit::Normal, widget->title(), &ok);

    if (ok && !newTitle.isEmpty()) {
        widget->setTitle(newTitle);
        emit layoutChanged();
    }
}

} // namespace netpulse::ui
