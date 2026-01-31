#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QSpinBox>

namespace netpulse::ui {

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private slots:
    void onAccept();
    void onApply();

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // General
    QComboBox* themeCombo_{nullptr};
    QCheckBox* startMinimizedCheck_{nullptr};
    QCheckBox* minimizeToTrayCheck_{nullptr};
    QCheckBox* startOnLoginCheck_{nullptr};

    // Monitoring defaults
    QSpinBox* defaultPingIntervalSpin_{nullptr};
    QSpinBox* warningThresholdSpin_{nullptr};
    QSpinBox* criticalThresholdSpin_{nullptr};

    // Alerts
    QSpinBox* alertLatencyWarningSpin_{nullptr};
    QSpinBox* alertLatencyCriticalSpin_{nullptr};
    QSpinBox* consecutiveFailuresSpin_{nullptr};
    QCheckBox* desktopNotificationsCheck_{nullptr};

    // Data retention
    QSpinBox* retentionDaysSpin_{nullptr};
    QCheckBox* autoCleanupCheck_{nullptr};

    // Port scanner
    QSpinBox* portScanConcurrencySpin_{nullptr};
    QSpinBox* portScanTimeoutSpin_{nullptr};
};

} // namespace netpulse::ui
