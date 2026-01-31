#include "ui/windows/PortScanDialog.hpp"

#include "app/Application.hpp"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>

namespace netpulse::ui {

PortScanDialog::PortScanDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Port Scanner");
    setMinimumSize(600, 500);

    setupUi();
}

void PortScanDialog::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);

    // Target settings
    auto* targetGroup = new QGroupBox("Target", this);
    auto* targetLayout = new QFormLayout(targetGroup);

    addressEdit_ = new QLineEdit(this);
    addressEdit_->setPlaceholderText("IP address or hostname");
    targetLayout->addRow("Address:", addressEdit_);

    portRangeCombo_ = new QComboBox(this);
    portRangeCombo_->addItem("Common Ports", static_cast<int>(core::PortRange::Common));
    portRangeCombo_->addItem("Web Ports", static_cast<int>(core::PortRange::Web));
    portRangeCombo_->addItem("Database Ports", static_cast<int>(core::PortRange::Database));
    portRangeCombo_->addItem("All Ports (1-65535)", static_cast<int>(core::PortRange::All));
    portRangeCombo_->addItem("Custom", static_cast<int>(core::PortRange::Custom));
    connect(portRangeCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &PortScanDialog::onPortRangeChanged);
    targetLayout->addRow("Port Range:", portRangeCombo_);

    customPortsEdit_ = new QLineEdit(this);
    customPortsEdit_->setPlaceholderText("e.g., 22,80,443,8080-8090");
    customPortsEdit_->setEnabled(false);
    targetLayout->addRow("Custom Ports:", customPortsEdit_);

    mainLayout->addWidget(targetGroup);

    // Options
    auto* optionsGroup = new QGroupBox("Options", this);
    auto* optionsLayout = new QFormLayout(optionsGroup);

    auto& config = app::Application::instance().config().config();

    concurrencySpin_ = new QSpinBox(this);
    concurrencySpin_->setRange(10, 1000);
    concurrencySpin_->setValue(config.portScanConcurrency);
    optionsLayout->addRow("Concurrency:", concurrencySpin_);

    timeoutSpin_ = new QSpinBox(this);
    timeoutSpin_->setRange(100, 10000);
    timeoutSpin_->setValue(config.portScanTimeoutMs);
    timeoutSpin_->setSuffix(" ms");
    optionsLayout->addRow("Timeout:", timeoutSpin_);

    mainLayout->addWidget(optionsGroup);

    // Results
    auto* resultsGroup = new QGroupBox("Results", this);
    auto* resultsLayout = new QVBoxLayout(resultsGroup);

    resultsTable_ = new QTableWidget(this);
    resultsTable_->setColumnCount(3);
    resultsTable_->setHorizontalHeaderLabels({"Port", "State", "Service"});
    resultsTable_->horizontalHeader()->setStretchLastSection(true);
    resultsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resultsLayout->addWidget(resultsTable_);

    progressBar_ = new QProgressBar(this);
    progressBar_->setValue(0);
    resultsLayout->addWidget(progressBar_);

    statusLabel_ = new QLabel("Ready", this);
    resultsLayout->addWidget(statusLabel_);

    mainLayout->addWidget(resultsGroup);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    startButton_ = new QPushButton("Start Scan", this);
    connect(startButton_, &QPushButton::clicked, this, &PortScanDialog::onStartScan);
    buttonLayout->addWidget(startButton_);

    cancelButton_ = new QPushButton("Cancel", this);
    cancelButton_->setEnabled(false);
    connect(cancelButton_, &QPushButton::clicked, this, &PortScanDialog::onCancelScan);
    buttonLayout->addWidget(cancelButton_);

    buttonLayout->addStretch();

    auto* closeButton = new QPushButton("Close", this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(closeButton);

    mainLayout->addLayout(buttonLayout);
}

void PortScanDialog::onPortRangeChanged(int index) {
    auto range = static_cast<core::PortRange>(portRangeCombo_->itemData(index).toInt());
    customPortsEdit_->setEnabled(range == core::PortRange::Custom);
}

void PortScanDialog::onStartScan() {
    QString address = addressEdit_->text().trimmed();
    if (address.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a target address.");
        return;
    }

    core::PortScanConfig config;
    config.targetAddress = address.toStdString();
    config.range =
        static_cast<core::PortRange>(portRangeCombo_->currentData().toInt());
    config.maxConcurrency = concurrencySpin_->value();
    config.timeout = std::chrono::milliseconds(timeoutSpin_->value());

    if (config.range == core::PortRange::Custom) {
        QString customPorts = customPortsEdit_->text().trimmed();
        if (customPorts.isEmpty()) {
            QMessageBox::warning(this, "Error", "Please enter custom ports.");
            return;
        }

        // Parse custom ports (e.g., "22,80,443,8080-8090")
        for (const QString& part : customPorts.split(',')) {
            if (part.contains('-')) {
                QStringList range = part.split('-');
                if (range.size() == 2) {
                    int start = range[0].toInt();
                    int end = range[1].toInt();
                    for (int p = start; p <= end && p <= 65535; ++p) {
                        config.customPorts.push_back(static_cast<uint16_t>(p));
                    }
                }
            } else {
                int port = part.toInt();
                if (port > 0 && port <= 65535) {
                    config.customPorts.push_back(static_cast<uint16_t>(port));
                }
            }
        }
    }

    resultsTable_->setRowCount(0);
    progressBar_->setValue(0);
    updateUiForScanning(true);

    auto& scanner = app::Application::instance().portScanner();

    scanner.scanAsync(
        config,
        [this](const core::PortScanResult& result) {
            QMetaObject::invokeMethod(this, [this, result]() { onScanResult(result); },
                                      Qt::QueuedConnection);
        },
        [this](const core::PortScanProgress& progress) {
            QMetaObject::invokeMethod(
                this,
                [this, scanned = progress.scannedPorts, total = progress.totalPorts,
                 open = progress.openPorts]() { onScanProgress(scanned, total, open); },
                Qt::QueuedConnection);
        },
        [this](const std::vector<core::PortScanResult>&) {
            QMetaObject::invokeMethod(this, [this]() { onScanComplete(); }, Qt::QueuedConnection);
        });
}

void PortScanDialog::onCancelScan() {
    app::Application::instance().portScanner().cancel();
    statusLabel_->setText("Scan cancelled");
    updateUiForScanning(false);
}

void PortScanDialog::onScanResult(const core::PortScanResult& result) {
    int row = resultsTable_->rowCount();
    resultsTable_->insertRow(row);

    resultsTable_->setItem(row, 0, new QTableWidgetItem(QString::number(result.port)));
    resultsTable_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(result.stateToString())));
    resultsTable_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(result.serviceName)));

    // Color code by state
    QColor color = result.state == core::PortState::Open ? QColor(0, 200, 0) : QColor(128, 128, 128);
    for (int col = 0; col < 3; ++col) {
        resultsTable_->item(row, col)->setForeground(color);
    }
}

void PortScanDialog::onScanProgress(int scanned, int total, int open) {
    if (total > 0) {
        progressBar_->setMaximum(total);
        progressBar_->setValue(scanned);
        statusLabel_->setText(
            QString("Scanning... %1/%2 (%3 open)").arg(scanned).arg(total).arg(open));
    }
}

void PortScanDialog::onScanComplete() {
    statusLabel_->setText(QString("Scan complete. Found %1 open port(s).").arg(resultsTable_->rowCount()));
    updateUiForScanning(false);
}

void PortScanDialog::updateUiForScanning(bool scanning) {
    scanning_ = scanning;
    startButton_->setEnabled(!scanning);
    cancelButton_->setEnabled(scanning);
    addressEdit_->setEnabled(!scanning);
    portRangeCombo_->setEnabled(!scanning);
    customPortsEdit_->setEnabled(!scanning && portRangeCombo_->currentData().toInt() ==
                                                  static_cast<int>(core::PortRange::Custom));
    concurrencySpin_->setEnabled(!scanning);
    timeoutSpin_->setEnabled(!scanning);
}

} // namespace netpulse::ui
