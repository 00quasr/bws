#pragma once

#include "core/types/Alert.hpp"
#include "core/types/Host.hpp"
#include "core/types/PingResult.hpp"
#include "ui/widgets/HostListWidget.hpp"
#include "ui/widgets/LatencyChartWidget.hpp"
#include "ui/widgets/dashboard/DashboardContainer.hpp"
#include "ui/widgets/dashboard/DashboardWidget.hpp"
#include "ui/widgets/dashboard/WidgetToolbar.hpp"

#include <QAction>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <memory>

namespace netpulse::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onAddHost();
    void onRemoveHost();
    void onEditHost();
    void onPortScan();
    void onSettings();
    void onExportData();
    void onAbout();

    void onHostSelected(int64_t hostId);
    void onPingResult(int64_t hostId, const core::PingResult& result);
    void onHostStatusChanged(int64_t hostId, core::HostStatus status);
    void onAlertTriggered(const core::Alert& alert);

    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void updateStatusBar();
    void refreshInterfaceStats();

    void onAddWidget(WidgetType type);
    void onResetLayout();
    void onDashboardLayoutChanged();

private:
    void setupUi();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupSystemTray();
    void setupConnections();

    void loadTheme(const QString& themeName);
    void saveWindowState();
    void restoreWindowState();
    void saveDashboardLayout();
    void loadDashboardLayout();

    // Main UI components
    QTabWidget* tabWidget_{nullptr};
    HostListWidget* hostListWidget_{nullptr};
    LatencyChartWidget* latencyChartWidget_{nullptr};

    // Dashboard widgets
    DashboardContainer* dashboardContainer_{nullptr};
    WidgetToolbar* widgetToolbar_{nullptr};

    // Network interface stats
    QWidget* interfaceStatsWidget_{nullptr};
    QTimer* interfaceStatsTimer_{nullptr};

    // Alert panel
    QWidget* alertPanel_{nullptr};
    QLabel* alertCountLabel_{nullptr};

    // System tray
    QSystemTrayIcon* trayIcon_{nullptr};
    QMenu* trayMenu_{nullptr};

    // Status bar components
    QLabel* statusLabel_{nullptr};
    QLabel* hostCountLabel_{nullptr};
    QLabel* networkStatusLabel_{nullptr};

    // Actions
    QAction* addHostAction_{nullptr};
    QAction* removeHostAction_{nullptr};
    QAction* editHostAction_{nullptr};
    QAction* portScanAction_{nullptr};
    QAction* settingsAction_{nullptr};
    QAction* exportAction_{nullptr};
    QAction* quitAction_{nullptr};

    // Current selection
    int64_t selectedHostId_{-1};

    // Widget layout tracking
    int nextWidgetRow_{0};
    int nextWidgetCol_{0};
};

} // namespace netpulse::ui
