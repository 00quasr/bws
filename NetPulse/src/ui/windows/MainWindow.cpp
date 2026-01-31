#include "ui/windows/MainWindow.hpp"

#include "app/Application.hpp"
#include "ui/windows/PortScanDialog.hpp"
#include "ui/windows/SettingsDialog.hpp"

#include <QCloseEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
#include <fstream>

namespace netpulse::ui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("NetPulse - Network Monitor");
    setMinimumSize(800, 600);

    setupUi();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupSystemTray();
    setupConnections();

    // Load theme
    auto& config = app::Application::instance().config().config();
    loadTheme(QString::fromStdString(config.theme));

    // Start status update timer
    auto* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    statusTimer->start(1000);

    // Interface stats timer
    interfaceStatsTimer_ = new QTimer(this);
    connect(interfaceStatsTimer_, &QTimer::timeout, this, &MainWindow::refreshInterfaceStats);
    interfaceStatsTimer_->start(2000);
}

MainWindow::~MainWindow() {
    saveWindowState();
}

void MainWindow::setupUi() {
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Create splitter for host list and main content
    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // Host list on the left
    hostListWidget_ = new HostListWidget(this);
    hostListWidget_->setMinimumWidth(250);
    hostListWidget_->setMaximumWidth(400);
    splitter->addWidget(hostListWidget_);

    // Tab widget for main content
    tabWidget_ = new QTabWidget(this);

    // Dashboard tab with widgets
    auto* dashboardWidget = new QWidget(this);
    auto* dashboardLayout = new QVBoxLayout(dashboardWidget);
    dashboardLayout->setContentsMargins(0, 0, 0, 0);
    dashboardLayout->setSpacing(0);

    widgetToolbar_ = new WidgetToolbar(dashboardWidget);
    dashboardLayout->addWidget(widgetToolbar_);

    dashboardContainer_ = new DashboardContainer(dashboardWidget);
    dashboardLayout->addWidget(dashboardContainer_, 1);

    tabWidget_->addTab(dashboardWidget, "Dashboard");

    // Classic chart tab for detailed host view
    auto* chartWidget = new QWidget(this);
    auto* chartLayout = new QVBoxLayout(chartWidget);
    latencyChartWidget_ = new LatencyChartWidget(this);
    chartLayout->addWidget(latencyChartWidget_);
    tabWidget_->addTab(chartWidget, "Host Details");

    // Network Interfaces tab
    interfaceStatsWidget_ = new QWidget(this);
    auto* ifaceLayout = new QVBoxLayout(interfaceStatsWidget_);
    ifaceLayout->addStretch();
    tabWidget_->addTab(interfaceStatsWidget_, "Interfaces");

    // Alerts tab
    alertPanel_ = new QWidget(this);
    auto* alertLayout = new QVBoxLayout(alertPanel_);
    alertCountLabel_ = new QLabel("No alerts", alertPanel_);
    alertLayout->addWidget(alertCountLabel_);
    alertLayout->addStretch();
    tabWidget_->addTab(alertPanel_, "Alerts");

    splitter->addWidget(tabWidget_);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter);

    loadDashboardLayout();
}

void MainWindow::setupMenuBar() {
    auto* fileMenu = menuBar()->addMenu("&File");

    addHostAction_ = fileMenu->addAction("&Add Host...", this, &MainWindow::onAddHost);
    addHostAction_->setShortcut(QKeySequence::New);

    removeHostAction_ = fileMenu->addAction("&Remove Host", this, &MainWindow::onRemoveHost);
    removeHostAction_->setShortcut(QKeySequence::Delete);
    removeHostAction_->setEnabled(false);

    editHostAction_ = fileMenu->addAction("&Edit Host...", this, &MainWindow::onEditHost);
    editHostAction_->setEnabled(false);

    fileMenu->addSeparator();

    exportAction_ = fileMenu->addAction("&Export Data...", this, &MainWindow::onExportData);
    exportAction_->setShortcut(QKeySequence("Ctrl+E"));
    exportAction_->setEnabled(false);

    fileMenu->addSeparator();

    quitAction_ = fileMenu->addAction("&Quit", this, &QMainWindow::close);
    quitAction_->setShortcut(QKeySequence::Quit);

    auto* toolsMenu = menuBar()->addMenu("&Tools");

    portScanAction_ = toolsMenu->addAction("&Port Scanner...", this, &MainWindow::onPortScan);
    portScanAction_->setShortcut(QKeySequence("Ctrl+P"));

    toolsMenu->addSeparator();

    settingsAction_ = toolsMenu->addAction("&Settings...", this, &MainWindow::onSettings);
    settingsAction_->setShortcut(QKeySequence::Preferences);

    auto* helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About NetPulse", this, &MainWindow::onAbout);
}

void MainWindow::setupToolBar() {
    auto* toolBar = addToolBar("Main");
    toolBar->setMovable(false);

    toolBar->addAction(addHostAction_);
    toolBar->addAction(removeHostAction_);
    toolBar->addSeparator();
    toolBar->addAction(portScanAction_);
    toolBar->addSeparator();
    toolBar->addAction(settingsAction_);
}

void MainWindow::setupStatusBar() {
    statusLabel_ = new QLabel("Ready", this);
    hostCountLabel_ = new QLabel("Hosts: 0", this);
    networkStatusLabel_ = new QLabel("Network: OK", this);

    statusBar()->addWidget(statusLabel_, 1);
    statusBar()->addPermanentWidget(hostCountLabel_);
    statusBar()->addPermanentWidget(networkStatusLabel_);
}

void MainWindow::setupSystemTray() {
    trayIcon_ = new QSystemTrayIcon(this);
    trayIcon_->setToolTip("NetPulse");

    trayMenu_ = new QMenu(this);
    trayMenu_->addAction("Show", this, &QMainWindow::show);
    trayMenu_->addAction("Hide", this, &QMainWindow::hide);
    trayMenu_->addSeparator();
    trayMenu_->addAction(quitAction_);

    trayIcon_->setContextMenu(trayMenu_);
    trayIcon_->show();

    connect(trayIcon_, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
}

void MainWindow::setupConnections() {
    auto& app = app::Application::instance();

    // Host selection
    connect(hostListWidget_, &HostListWidget::hostSelected, this, &MainWindow::onHostSelected);
    connect(hostListWidget_, &HostListWidget::hostDoubleClicked, this, &MainWindow::onEditHost);

    // Dashboard updates
    connect(&app.dashboardViewModel(), &viewmodels::DashboardViewModel::pingResultReceived, this,
            &MainWindow::onPingResult);
    connect(&app.dashboardViewModel(), &viewmodels::DashboardViewModel::hostStatusChanged, this,
            &MainWindow::onHostStatusChanged);

    // Alerts
    connect(&app.alertsViewModel(), &viewmodels::AlertsViewModel::alertTriggered, this,
            &MainWindow::onAlertTriggered);

    // Host changes
    connect(&app.hostMonitorViewModel(), &viewmodels::HostMonitorViewModel::hostAdded, this,
            [this](int64_t) { hostListWidget_->refreshHosts(); });
    connect(&app.hostMonitorViewModel(), &viewmodels::HostMonitorViewModel::hostRemoved, this,
            [this](int64_t) { hostListWidget_->refreshHosts(); });
    connect(&app.hostMonitorViewModel(), &viewmodels::HostMonitorViewModel::hostUpdated, this,
            [this](int64_t) { hostListWidget_->refreshHosts(); });

    // Dashboard widget toolbar
    connect(widgetToolbar_, &WidgetToolbar::addWidgetRequested, this, &MainWindow::onAddWidget);
    connect(widgetToolbar_, &WidgetToolbar::resetLayoutRequested, this, &MainWindow::onResetLayout);
    connect(dashboardContainer_, &DashboardContainer::layoutChanged, this,
            &MainWindow::onDashboardLayoutChanged);
}

void MainWindow::onAddHost() {
    bool ok;
    QString name = QInputDialog::getText(this, "Add Host", "Host name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty())
        return;

    QString address =
        QInputDialog::getText(this, "Add Host", "Address (IP or hostname):", QLineEdit::Normal, "", &ok);
    if (!ok || address.isEmpty())
        return;

    auto& vm = app::Application::instance().hostMonitorViewModel();
    vm.addHost(name.toStdString(), address.toStdString());

    // Start monitoring the new host
    app::Application::instance().dashboardViewModel().startMonitoring();

    statusLabel_->setText(QString("Added host: %1").arg(name));
}

void MainWindow::onRemoveHost() {
    if (selectedHostId_ < 0)
        return;

    auto& vm = app::Application::instance().hostMonitorViewModel();
    auto host = vm.getHost(selectedHostId_);
    if (!host)
        return;

    auto reply = QMessageBox::question(
        this, "Remove Host",
        QString("Are you sure you want to remove '%1'?").arg(QString::fromStdString(host->name)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        vm.removeHost(selectedHostId_);
        selectedHostId_ = -1;
        removeHostAction_->setEnabled(false);
        editHostAction_->setEnabled(false);
        exportAction_->setEnabled(false);
        latencyChartWidget_->clearChart();
    }
}

void MainWindow::onEditHost() {
    if (selectedHostId_ < 0)
        return;

    auto& vm = app::Application::instance().hostMonitorViewModel();
    auto host = vm.getHost(selectedHostId_);
    if (!host)
        return;

    bool ok;
    QString name = QInputDialog::getText(this, "Edit Host", "Host name:", QLineEdit::Normal,
                                         QString::fromStdString(host->name), &ok);
    if (!ok)
        return;

    host->name = name.toStdString();
    vm.updateHost(*host);
}

void MainWindow::onPortScan() {
    PortScanDialog dialog(this);
    dialog.exec();
}

void MainWindow::onSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        auto& config = app::Application::instance().config().config();
        loadTheme(QString::fromStdString(config.theme));
    }
}

void MainWindow::onExportData() {
    if (selectedHostId_ < 0)
        return;

    QString fileName = QFileDialog::getSaveFileName(this, "Export Data", "",
                                                    "JSON Files (*.json);;CSV Files (*.csv)");
    if (fileName.isEmpty())
        return;

    auto& vm = app::Application::instance().hostMonitorViewModel();
    QString format = fileName.endsWith(".csv") ? "csv" : "json";
    std::string exportedData = vm.exportHostData(selectedHostId_, format.toStdString());

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(exportedData.c_str());
        statusLabel_->setText(QString("Exported data to %1").arg(fileName));
    }
}

void MainWindow::onAbout() {
    QMessageBox::about(this, "About NetPulse",
                       "<h2>NetPulse</h2>"
                       "<p>Version 1.0.0</p>"
                       "<p>A network monitoring dashboard.</p>"
                       "<p>Features:</p>"
                       "<ul>"
                       "<li>Real-time host monitoring</li>"
                       "<li>Latency graphs and statistics</li>"
                       "<li>Port scanning</li>"
                       "<li>Alert notifications</li>"
                       "</ul>");
}

void MainWindow::onHostSelected(int64_t hostId) {
    selectedHostId_ = hostId;
    removeHostAction_->setEnabled(hostId >= 0);
    editHostAction_->setEnabled(hostId >= 0);
    exportAction_->setEnabled(hostId >= 0);

    if (hostId >= 0) {
        latencyChartWidget_->setHost(hostId);

        // Load existing data
        auto& vm = app::Application::instance().dashboardViewModel();
        auto results = vm.getRecentResults(hostId, 100);

        latencyChartWidget_->clearChart();
        for (auto it = results.rbegin(); it != results.rend(); ++it) {
            latencyChartWidget_->addDataPoint(*it);
        }
    }
}

void MainWindow::onPingResult(int64_t hostId, const core::PingResult& result) {
    if (hostId == selectedHostId_) {
        latencyChartWidget_->addDataPoint(result);
    }

    hostListWidget_->updateHostStatus(hostId);
    hostListWidget_->updateHostSparkline(hostId, result);
}

void MainWindow::onHostStatusChanged(int64_t hostId, core::HostStatus /*status*/) {
    hostListWidget_->updateHostStatus(hostId);

    // Update tray icon based on overall status
    auto& vm = app::Application::instance().dashboardViewModel();
    int down = vm.hostsDown();

    if (down > 0) {
        trayIcon_->setToolTip(QString("NetPulse - %1 host(s) down").arg(down));
    } else {
        trayIcon_->setToolTip("NetPulse - All hosts OK");
    }
}

void MainWindow::onAlertTriggered(const core::Alert& alert) {
    if (app::Application::instance().config().config().desktopNotifications) {
        trayIcon_->showMessage(QString::fromStdString(alert.title),
                               QString::fromStdString(alert.message),
                               alert.severity == core::AlertSeverity::Critical
                                   ? QSystemTrayIcon::Critical
                                   : QSystemTrayIcon::Warning,
                               5000);
    }

    auto& vm = app::Application::instance().alertsViewModel();
    alertCountLabel_->setText(QString("Alerts: %1 unacknowledged").arg(vm.unacknowledgedCount()));
}

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            show();
            activateWindow();
        }
    }
}

void MainWindow::updateStatusBar() {
    auto& vm = app::Application::instance().dashboardViewModel();
    int total = vm.hostCount();
    int up = vm.hostsUp();
    int down = vm.hostsDown();

    hostCountLabel_->setText(QString("Hosts: %1 (Up: %2, Down: %3)").arg(total).arg(up).arg(down));

    if (down > 0) {
        networkStatusLabel_->setText("<span style='color:red'>Network: Issues</span>");
    } else if (up > 0) {
        networkStatusLabel_->setText("<span style='color:green'>Network: OK</span>");
    } else {
        networkStatusLabel_->setText("Network: --");
    }
}

void MainWindow::refreshInterfaceStats() {
    auto interfaces = core::NetworkInterfaceEnumerator::enumerate();

    // Clear existing layout
    auto* layout = interfaceStatsWidget_->layout();
    if (layout) {
        QLayoutItem* item;
        while ((item = layout->takeAt(0)) != nullptr) {
            delete item->widget();
            delete item;
        }
    } else {
        layout = new QVBoxLayout(interfaceStatsWidget_);
    }

    for (const auto& iface : interfaces) {
        if (iface.isLoopback)
            continue;

        auto* card = new QWidget(interfaceStatsWidget_);
        auto* cardLayout = new QVBoxLayout(card);
        card->setStyleSheet("QWidget { border: 1px solid gray; padding: 10px; margin: 5px; }");

        cardLayout->addWidget(new QLabel(QString("<b>%1</b> (%2)")
                                             .arg(QString::fromStdString(iface.name))
                                             .arg(QString::fromStdString(iface.ipAddress))));
        cardLayout->addWidget(
            new QLabel(QString("RX: %1").arg(QString::fromStdString(iface.formatBytesReceived()))));
        cardLayout->addWidget(
            new QLabel(QString("TX: %1").arg(QString::fromStdString(iface.formatBytesSent()))));

        layout->addWidget(card);
    }

    static_cast<QVBoxLayout*>(layout)->addStretch();
}

void MainWindow::onAddWidget(WidgetType type) {
    int row = nextWidgetRow_;
    int col = nextWidgetCol_;

    dashboardContainer_->addWidget(type, row, col);

    nextWidgetCol_++;
    if (nextWidgetCol_ >= 3) {
        nextWidgetCol_ = 0;
        nextWidgetRow_++;
    }

    statusLabel_->setText(QString("Added %1 widget").arg(widgetTypeToString(type)));
}

void MainWindow::onResetLayout() {
    dashboardContainer_->loadDefaultLayout();
    nextWidgetRow_ = 2;
    nextWidgetCol_ = 0;
    statusLabel_->setText("Dashboard layout reset to default");
}

void MainWindow::onDashboardLayoutChanged() {
    saveDashboardLayout();
}

void MainWindow::saveDashboardLayout() {
    auto configs = dashboardContainer_->getConfiguration();

    nlohmann::json layoutJson = nlohmann::json::array();
    for (const auto& config : configs) {
        layoutJson.push_back(config.toJson());
    }

    auto& configManager = app::Application::instance().config();
    std::string configPath = configManager.configDir() + "/dashboard_layout.json";

    std::ofstream file(configPath);
    if (file.is_open()) {
        file << layoutJson.dump(2);
    }
}

void MainWindow::loadDashboardLayout() {
    auto& configManager = app::Application::instance().config();
    std::string configPath = configManager.configDir() + "/dashboard_layout.json";

    std::ifstream file(configPath);
    if (file.is_open()) {
        try {
            nlohmann::json layoutJson;
            file >> layoutJson;

            std::vector<WidgetConfig> configs;
            for (const auto& item : layoutJson) {
                configs.push_back(WidgetConfig::fromJson(item));
            }

            if (!configs.empty()) {
                dashboardContainer_->loadConfiguration(configs);

                int maxRow = 0;
                int maxCol = 0;
                for (const auto& config : configs) {
                    maxRow = std::max(maxRow, config.row + config.rowSpan);
                    maxCol = std::max(maxCol, config.col + 1);
                }
                nextWidgetRow_ = maxRow;
                nextWidgetCol_ = maxCol >= 3 ? 0 : maxCol;
                if (maxCol >= 3) {
                    nextWidgetRow_++;
                }
                return;
            }
        } catch (...) {
        }
    }

    dashboardContainer_->loadDefaultLayout();
    nextWidgetRow_ = 2;
    nextWidgetCol_ = 0;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    auto& config = app::Application::instance().config().config();

    if (config.minimizeToTray && trayIcon_->isVisible()) {
        hide();
        event->ignore();
    } else {
        saveWindowState();
        saveDashboardLayout();
        event->accept();
    }
}

void MainWindow::loadTheme(const QString& themeName) {
    QString themeFile = QString(":/themes/%1Theme.qss").arg(themeName.toLower().replace(0, 1, themeName[0].toUpper()));

    QFile file(themeFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setStyleSheet(file.readAll());
    }
}

void MainWindow::saveWindowState() {
    auto& config = app::Application::instance().config().config();

    if (!isMaximized()) {
        auto geom = geometry();
        config.windowX = geom.x();
        config.windowY = geom.y();
        config.windowWidth = geom.width();
        config.windowHeight = geom.height();
    }
    config.windowMaximized = isMaximized();

    app::Application::instance().config().save();
}

void MainWindow::restoreWindowState() {
    auto& config = app::Application::instance().config().config();

    if (config.windowMaximized) {
        showMaximized();
    } else {
        setGeometry(config.windowX, config.windowY, config.windowWidth, config.windowHeight);
    }
}

} // namespace netpulse::ui
