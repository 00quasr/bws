#pragma once

#include "core/types/PortScanResult.hpp"

#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>

namespace netpulse::ui {

class PortScanDialog : public QDialog {
    Q_OBJECT

public:
    explicit PortScanDialog(QWidget* parent = nullptr);

private slots:
    void onStartScan();
    void onCancelScan();
    void onPortRangeChanged(int index);
    void onScanResult(const core::PortScanResult& result);
    void onScanProgress(int scanned, int total, int open);
    void onScanComplete();

private:
    void setupUi();
    void updateUiForScanning(bool scanning);

    QLineEdit* addressEdit_{nullptr};
    QComboBox* portRangeCombo_{nullptr};
    QLineEdit* customPortsEdit_{nullptr};
    QSpinBox* concurrencySpin_{nullptr};
    QSpinBox* timeoutSpin_{nullptr};

    QTableWidget* resultsTable_{nullptr};
    QProgressBar* progressBar_{nullptr};
    QLabel* statusLabel_{nullptr};

    QPushButton* startButton_{nullptr};
    QPushButton* cancelButton_{nullptr};

    bool scanning_{false};
};

} // namespace netpulse::ui
