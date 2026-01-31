#pragma once

#include "core/types/Alert.hpp"
#include "ui/widgets/dashboard/DashboardWidget.hpp"

#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QTimer>

namespace netpulse::ui {

class AlertsWidget : public DashboardWidget {
    Q_OBJECT

public:
    explicit AlertsWidget(QWidget* parent = nullptr);

    [[nodiscard]] WidgetType widgetType() const override { return WidgetType::Alerts; }

    [[nodiscard]] nlohmann::json settings() const override;
    void applySettings(const nlohmann::json& settings) override;

    void refresh() override;

signals:
    void alertClicked(int64_t alertId);

private slots:
    void onFilterChanged();
    void onSearchTextChanged(const QString& text);

private:
    void setupFilterBar();
    [[nodiscard]] core::AlertFilter buildFilter() const;

    QLineEdit* searchEdit_{nullptr};
    QComboBox* severityCombo_{nullptr};
    QComboBox* typeCombo_{nullptr};
    QComboBox* statusCombo_{nullptr};
    QListWidget* alertList_{nullptr};
    QTimer* refreshTimer_{nullptr};
    QTimer* searchDebounceTimer_{nullptr};
    int maxAlerts_{10};
};

} // namespace netpulse::ui
