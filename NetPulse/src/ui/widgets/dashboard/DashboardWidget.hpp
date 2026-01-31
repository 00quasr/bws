#pragma once

#include <QFrame>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <nlohmann/json.hpp>

namespace netpulse::ui {

enum class WidgetType {
    Statistics,
    HostStatus,
    Alerts,
    NetworkOverview,
    LatencyHistory,
    Topology
};

struct WidgetConfig {
    WidgetType type{WidgetType::Statistics};
    QString title{"Widget"};
    int row{0};
    int col{0};
    int rowSpan{1};
    int colSpan{1};
    nlohmann::json settings;

    [[nodiscard]] nlohmann::json toJson() const;
    static WidgetConfig fromJson(const nlohmann::json& j);
};

class DashboardWidget : public QFrame {
    Q_OBJECT

public:
    explicit DashboardWidget(const QString& title, QWidget* parent = nullptr);
    ~DashboardWidget() override = default;

    [[nodiscard]] virtual WidgetType widgetType() const = 0;
    [[nodiscard]] QString title() const { return title_; }
    void setTitle(const QString& title);

    [[nodiscard]] virtual nlohmann::json settings() const { return {}; }
    virtual void applySettings(const nlohmann::json& settings) { (void)settings; }

    virtual void refresh() = 0;

signals:
    void removeRequested();
    void settingsRequested();

protected:
    void setupBaseUi();
    void setContentWidget(QWidget* content);
    void contextMenuEvent(QContextMenuEvent* event) override;

    QLabel* titleLabel_{nullptr};
    QVBoxLayout* contentLayout_{nullptr};

private:
    QString title_;
    QPushButton* menuButton_{nullptr};
};

QString widgetTypeToString(WidgetType type);
WidgetType widgetTypeFromString(const QString& str);

} // namespace netpulse::ui
