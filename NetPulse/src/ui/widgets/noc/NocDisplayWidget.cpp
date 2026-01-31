#include "ui/widgets/noc/NocDisplayWidget.hpp"

#include "app/Application.hpp"

#include <QDateTime>
#include <QKeyEvent>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>

namespace netpulse::ui {

NocDisplayWidget::NocDisplayWidget(QWidget* parent) : QWidget(parent) {
    setObjectName("NocDisplayWidget");
    setAttribute(Qt::WA_StyledBackground, true);

    setupUi();

    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &NocDisplayWidget::refresh);
    refreshTimer_->start(2000);

    clockTimer_ = new QTimer(this);
    connect(clockTimer_, &QTimer::timeout, this, [this]() {
        clockLabel_->setText(QDateTime::currentDateTime().toString("hh:mm:ss"));
    });
    clockTimer_->start(1000);

    refresh();
}

void NocDisplayWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    setupHeader();
    setupHostGrid();
    setupFooter();

    mainLayout->addWidget(titleLabel_->parentWidget());

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("NocScrollArea");
    scrollArea->setWidget(hostGridWidget_);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    mainLayout->addWidget(scrollArea, 1);

    auto* footerWidget = summaryLabel_->parentWidget();
    mainLayout->addWidget(footerWidget);
}

void NocDisplayWidget::setupHeader() {
    auto* headerWidget = new QWidget(this);
    headerWidget->setObjectName("NocHeader");
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);

    titleLabel_ = new QLabel("NETWORK OPERATIONS CENTER", this);
    titleLabel_->setObjectName("NocTitle");
    headerLayout->addWidget(titleLabel_);

    headerLayout->addStretch();

    clockLabel_ = new QLabel(QDateTime::currentDateTime().toString("hh:mm:ss"), this);
    clockLabel_->setObjectName("NocClock");
    headerLayout->addWidget(clockLabel_);
}

void NocDisplayWidget::setupHostGrid() {
    hostGridWidget_ = new QWidget(this);
    hostGridWidget_->setObjectName("NocHostGrid");
    hostGridLayout_ = new QGridLayout(hostGridWidget_);
    hostGridLayout_->setContentsMargins(0, 0, 0, 0);
    hostGridLayout_->setSpacing(16);

    updateHostCards();
}

void NocDisplayWidget::setupFooter() {
    auto* footerWidget = new QWidget(this);
    footerWidget->setObjectName("NocFooter");
    auto* footerLayout = new QHBoxLayout(footerWidget);
    footerLayout->setContentsMargins(0, 0, 0, 0);

    summaryLabel_ = new QLabel("Loading...", this);
    summaryLabel_->setObjectName("NocSummary");
    footerLayout->addWidget(summaryLabel_);

    footerLayout->addStretch();

    uptimeLabel_ = new QLabel("Press ESC or F11 to exit NOC mode", this);
    uptimeLabel_->setObjectName("NocHint");
    footerLayout->addWidget(uptimeLabel_);
}

void NocDisplayWidget::updateHostCards() {
    auto& vm = app::Application::instance().dashboardViewModel();
    auto hosts = vm.getHosts();

    for (auto* card : hostCards_) {
        hostGridLayout_->removeWidget(card);
        card->deleteLater();
    }
    hostCards_.clear();

    int row = 0;
    int col = 0;

    for (const auto& host : hosts) {
        if (!host.enabled) {
            continue;
        }

        auto* card = new NocHostCard(host, hostGridWidget_);
        hostCards_.push_back(card);
        hostGridLayout_->addWidget(card, row, col);

        col++;
        if (col >= COLUMNS) {
            col = 0;
            row++;
        }
    }

    for (int c = 0; c < COLUMNS; ++c) {
        hostGridLayout_->setColumnStretch(c, 1);
    }
}

void NocDisplayWidget::updateSummary() {
    auto& vm = app::Application::instance().dashboardViewModel();

    int total = vm.hostCount();
    int up = vm.hostsUp();
    int down = vm.hostsDown();
    int warning = total - up - down;

    QString summaryText = QString("HOSTS: %1 Total | %2 Up | %3 Down | %4 Warning")
                              .arg(total)
                              .arg(up)
                              .arg(down)
                              .arg(warning);

    summaryLabel_->setText(summaryText);

    if (down > 0) {
        summaryLabel_->setProperty("statusClass", "critical");
    } else if (warning > 0) {
        summaryLabel_->setProperty("statusClass", "warning");
    } else {
        summaryLabel_->setProperty("statusClass", "healthy");
    }

    summaryLabel_->style()->unpolish(summaryLabel_);
    summaryLabel_->style()->polish(summaryLabel_);
}

void NocDisplayWidget::refresh() {
    auto& vm = app::Application::instance().dashboardViewModel();
    auto hosts = vm.getHosts();

    if (static_cast<size_t>(vm.hostCount()) != hostCards_.size()) {
        updateHostCards();
    }

    for (auto* card : hostCards_) {
        for (const auto& host : hosts) {
            if (host.id == card->hostId()) {
                auto results = vm.getRecentResults(host.id, 1);
                double latency = 0.0;
                if (!results.empty() && results[0].success) {
                    latency = static_cast<double>(results[0].latency.count()) / 1000.0;
                }
                card->updateStatus(host.status, latency);
                break;
            }
        }
    }

    updateSummary();
}

void NocDisplayWidget::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_F11) {
        emit exitRequested();
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void NocDisplayWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    emit exitRequested();
    QWidget::mouseDoubleClickEvent(event);
}

} // namespace netpulse::ui
