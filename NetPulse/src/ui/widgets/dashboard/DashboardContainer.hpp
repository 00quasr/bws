#pragma once

#include "ui/widgets/dashboard/DashboardWidget.hpp"
#include "ui/widgets/dashboard/WidgetFactory.hpp"

#include <QGridLayout>
#include <QScrollArea>
#include <QWidget>
#include <memory>
#include <vector>

namespace netpulse::ui {

class DashboardContainer : public QWidget {
    Q_OBJECT

public:
    explicit DashboardContainer(QWidget* parent = nullptr);

    void addWidget(WidgetType type, int row, int col, int rowSpan = 1, int colSpan = 1);
    void addWidget(const WidgetConfig& config);
    void removeWidget(DashboardWidget* widget);
    void clearWidgets();

    [[nodiscard]] std::vector<WidgetConfig> getConfiguration() const;
    void loadConfiguration(const std::vector<WidgetConfig>& configs);

    void loadDefaultLayout();

signals:
    void widgetRemoved();
    void layoutChanged();

private slots:
    void onWidgetRemoveRequested();
    void onWidgetSettingsRequested();

private:
    void setupUi();
    void updateGridPositions();

    QScrollArea* scrollArea_{nullptr};
    QWidget* containerWidget_{nullptr};
    QGridLayout* gridLayout_{nullptr};

    struct WidgetEntry {
        std::unique_ptr<DashboardWidget> widget;
        int row;
        int col;
        int rowSpan;
        int colSpan;
    };

    std::vector<WidgetEntry> widgets_;
};

} // namespace netpulse::ui
