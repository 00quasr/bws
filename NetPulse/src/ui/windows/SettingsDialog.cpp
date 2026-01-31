#include "ui/windows/SettingsDialog.hpp"

#include "app/Application.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

namespace netpulse::ui {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings");
    setMinimumSize(450, 500);

    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabWidget = new QTabWidget(this);

    // General tab
    auto* generalTab = new QWidget(this);
    auto* generalLayout = new QVBoxLayout(generalTab);

    auto* appearanceGroup = new QGroupBox("Appearance", generalTab);
    auto* appearanceLayout = new QFormLayout(appearanceGroup);

    themeCombo_ = new QComboBox(this);
    themeCombo_->addItem("Dark");
    themeCombo_->addItem("Light");
    appearanceLayout->addRow("Theme:", themeCombo_);

    generalLayout->addWidget(appearanceGroup);

    auto* startupGroup = new QGroupBox("Startup", generalTab);
    auto* startupLayout = new QVBoxLayout(startupGroup);

    startMinimizedCheck_ = new QCheckBox("Start minimized", this);
    minimizeToTrayCheck_ = new QCheckBox("Minimize to system tray", this);
    startOnLoginCheck_ = new QCheckBox("Start on login", this);

    startupLayout->addWidget(startMinimizedCheck_);
    startupLayout->addWidget(minimizeToTrayCheck_);
    startupLayout->addWidget(startOnLoginCheck_);

    generalLayout->addWidget(startupGroup);
    generalLayout->addStretch();

    tabWidget->addTab(generalTab, "General");

    // Monitoring tab
    auto* monitoringTab = new QWidget(this);
    auto* monitoringLayout = new QVBoxLayout(monitoringTab);

    auto* defaultsGroup = new QGroupBox("Default Values", monitoringTab);
    auto* defaultsLayout = new QFormLayout(defaultsGroup);

    defaultPingIntervalSpin_ = new QSpinBox(this);
    defaultPingIntervalSpin_->setRange(5, 3600);
    defaultPingIntervalSpin_->setSuffix(" seconds");
    defaultsLayout->addRow("Ping interval:", defaultPingIntervalSpin_);

    warningThresholdSpin_ = new QSpinBox(this);
    warningThresholdSpin_->setRange(1, 10000);
    warningThresholdSpin_->setSuffix(" ms");
    defaultsLayout->addRow("Warning threshold:", warningThresholdSpin_);

    criticalThresholdSpin_ = new QSpinBox(this);
    criticalThresholdSpin_->setRange(1, 30000);
    criticalThresholdSpin_->setSuffix(" ms");
    defaultsLayout->addRow("Critical threshold:", criticalThresholdSpin_);

    monitoringLayout->addWidget(defaultsGroup);
    monitoringLayout->addStretch();

    tabWidget->addTab(monitoringTab, "Monitoring");

    // Alerts tab
    auto* alertsTab = new QWidget(this);
    auto* alertsLayout = new QVBoxLayout(alertsTab);

    auto* thresholdsGroup = new QGroupBox("Alert Thresholds", alertsTab);
    auto* thresholdsLayout = new QFormLayout(thresholdsGroup);

    alertLatencyWarningSpin_ = new QSpinBox(this);
    alertLatencyWarningSpin_->setRange(1, 10000);
    alertLatencyWarningSpin_->setSuffix(" ms");
    thresholdsLayout->addRow("Latency warning:", alertLatencyWarningSpin_);

    alertLatencyCriticalSpin_ = new QSpinBox(this);
    alertLatencyCriticalSpin_->setRange(1, 30000);
    alertLatencyCriticalSpin_->setSuffix(" ms");
    thresholdsLayout->addRow("Latency critical:", alertLatencyCriticalSpin_);

    consecutiveFailuresSpin_ = new QSpinBox(this);
    consecutiveFailuresSpin_->setRange(1, 100);
    thresholdsLayout->addRow("Failures for down:", consecutiveFailuresSpin_);

    alertsLayout->addWidget(thresholdsGroup);

    auto* notificationsGroup = new QGroupBox("Notifications", alertsTab);
    auto* notificationsLayout = new QVBoxLayout(notificationsGroup);

    desktopNotificationsCheck_ = new QCheckBox("Show desktop notifications", this);
    notificationsLayout->addWidget(desktopNotificationsCheck_);

    alertsLayout->addWidget(notificationsGroup);
    alertsLayout->addStretch();

    tabWidget->addTab(alertsTab, "Alerts");

    // Data tab
    auto* dataTab = new QWidget(this);
    auto* dataLayout = new QVBoxLayout(dataTab);

    auto* retentionGroup = new QGroupBox("Data Retention", dataTab);
    auto* retentionLayout = new QFormLayout(retentionGroup);

    retentionDaysSpin_ = new QSpinBox(this);
    retentionDaysSpin_->setRange(1, 365);
    retentionDaysSpin_->setSuffix(" days");
    retentionLayout->addRow("Keep data for:", retentionDaysSpin_);

    autoCleanupCheck_ = new QCheckBox("Auto-cleanup old data", this);
    retentionLayout->addRow("", autoCleanupCheck_);

    dataLayout->addWidget(retentionGroup);

    auto* portScanGroup = new QGroupBox("Port Scanner", dataTab);
    auto* portScanLayout = new QFormLayout(portScanGroup);

    portScanConcurrencySpin_ = new QSpinBox(this);
    portScanConcurrencySpin_->setRange(10, 1000);
    portScanLayout->addRow("Max concurrent:", portScanConcurrencySpin_);

    portScanTimeoutSpin_ = new QSpinBox(this);
    portScanTimeoutSpin_->setRange(100, 10000);
    portScanTimeoutSpin_->setSuffix(" ms");
    portScanLayout->addRow("Connection timeout:", portScanTimeoutSpin_);

    dataLayout->addWidget(portScanGroup);
    dataLayout->addStretch();

    tabWidget->addTab(dataTab, "Data");

    mainLayout->addWidget(tabWidget);

    // Buttons
    auto* buttonBox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply,
                             this);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            &SettingsDialog::onApply);

    mainLayout->addWidget(buttonBox);
}

void SettingsDialog::loadSettings() {
    auto& config = app::Application::instance().config().config();

    // General
    themeCombo_->setCurrentText(QString::fromStdString(config.theme));
    startMinimizedCheck_->setChecked(config.startMinimized);
    minimizeToTrayCheck_->setChecked(config.minimizeToTray);
    startOnLoginCheck_->setChecked(config.startOnLogin);

    // Monitoring
    defaultPingIntervalSpin_->setValue(config.defaultPingIntervalSeconds);
    warningThresholdSpin_->setValue(config.defaultWarningThresholdMs);
    criticalThresholdSpin_->setValue(config.defaultCriticalThresholdMs);

    // Alerts
    alertLatencyWarningSpin_->setValue(config.alertThresholds.latencyWarningMs);
    alertLatencyCriticalSpin_->setValue(config.alertThresholds.latencyCriticalMs);
    consecutiveFailuresSpin_->setValue(config.alertThresholds.consecutiveFailuresForDown);
    desktopNotificationsCheck_->setChecked(config.desktopNotifications);

    // Data
    retentionDaysSpin_->setValue(config.dataRetentionDays);
    autoCleanupCheck_->setChecked(config.autoCleanup);
    portScanConcurrencySpin_->setValue(config.portScanConcurrency);
    portScanTimeoutSpin_->setValue(config.portScanTimeoutMs);
}

void SettingsDialog::saveSettings() {
    auto& config = app::Application::instance().config().config();

    // General
    config.theme = themeCombo_->currentText().toLower().toStdString();
    config.startMinimized = startMinimizedCheck_->isChecked();
    config.minimizeToTray = minimizeToTrayCheck_->isChecked();
    config.startOnLogin = startOnLoginCheck_->isChecked();

    // Monitoring
    config.defaultPingIntervalSeconds = defaultPingIntervalSpin_->value();
    config.defaultWarningThresholdMs = warningThresholdSpin_->value();
    config.defaultCriticalThresholdMs = criticalThresholdSpin_->value();

    // Alerts
    config.alertThresholds.latencyWarningMs = alertLatencyWarningSpin_->value();
    config.alertThresholds.latencyCriticalMs = alertLatencyCriticalSpin_->value();
    config.alertThresholds.consecutiveFailuresForDown = consecutiveFailuresSpin_->value();
    config.desktopNotifications = desktopNotificationsCheck_->isChecked();

    // Data
    config.dataRetentionDays = retentionDaysSpin_->value();
    config.autoCleanup = autoCleanupCheck_->isChecked();
    config.portScanConcurrency = portScanConcurrencySpin_->value();
    config.portScanTimeoutMs = portScanTimeoutSpin_->value();

    // Update alert thresholds in viewmodel
    app::Application::instance().alertsViewModel().setThresholds(config.alertThresholds);

    app::Application::instance().config().save();
}

void SettingsDialog::onAccept() {
    saveSettings();
    accept();
}

void SettingsDialog::onApply() {
    saveSettings();
}

} // namespace netpulse::ui
