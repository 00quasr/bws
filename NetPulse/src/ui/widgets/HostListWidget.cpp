#include "ui/widgets/HostListWidget.hpp"

#include "app/Application.hpp"
#include "viewmodels/DashboardViewModel.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>

namespace netpulse::ui {

namespace {
constexpr int ITEM_TYPE_HOST = 1;
constexpr int ITEM_TYPE_GROUP = 2;
constexpr int DATA_ROLE_ID = Qt::UserRole;
constexpr int DATA_ROLE_TYPE = Qt::UserRole + 1;
constexpr int COLUMN_HOST = 0;
constexpr int COLUMN_SPARKLINE = 1;
constexpr int SPARKLINE_DATA_POINTS = 30;
} // namespace

HostListWidget::HostListWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    refreshHosts();
}

void HostListWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* headerLabel = new QLabel("<b>Monitored Hosts</b>", this);
    headerLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(headerLabel);

    treeWidget_ = new QTreeWidget(this);
    treeWidget_->setColumnCount(2);
    treeWidget_->setHeaderHidden(true);
    treeWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    treeWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    treeWidget_->setIndentation(20);
    treeWidget_->setAnimated(true);
    treeWidget_->header()->setStretchLastSection(false);
    treeWidget_->header()->setSectionResizeMode(COLUMN_HOST, QHeaderView::Stretch);
    treeWidget_->header()->setSectionResizeMode(COLUMN_SPARKLINE, QHeaderView::Fixed);
    treeWidget_->header()->resizeSection(COLUMN_SPARKLINE, 85);
    layout->addWidget(treeWidget_);

    connect(treeWidget_, &QTreeWidget::itemSelectionChanged, this,
            &HostListWidget::onItemSelectionChanged);
    connect(treeWidget_, &QTreeWidget::itemDoubleClicked, this,
            &HostListWidget::onItemDoubleClicked);
    connect(treeWidget_, &QTreeWidget::customContextMenuRequested, this,
            &HostListWidget::onContextMenuRequested);
}

void HostListWidget::refreshHosts() {
    treeWidget_->clear();
    hostItems_.clear();
    groupItems_.clear();
    statusIndicators_.clear();
    sparklines_.clear();

    populateTree();
    treeWidget_->expandAll();
}

void HostListWidget::populateTree() {
    auto& groupVm = app::Application::instance().hostGroupViewModel();

    // Add root groups first
    auto rootGroups = groupVm.getRootGroups();
    for (const auto& group : rootGroups) {
        addGroupToTree(group, nullptr);
    }

    // Add ungrouped hosts at root level
    auto ungroupedHosts = groupVm.getUngroupedHosts();
    for (const auto& host : ungroupedHosts) {
        auto* item = createHostItem(host);
        treeWidget_->addTopLevelItem(item);
    }
}

void HostListWidget::addGroupToTree(const core::HostGroup& group, QTreeWidgetItem* parent) {
    auto& groupVm = app::Application::instance().hostGroupViewModel();

    auto* groupItem = new QTreeWidgetItem();
    groupItem->setText(0, QString::fromStdString("📁 " + group.name));
    groupItem->setData(0, DATA_ROLE_ID, QVariant::fromValue(group.id));
    groupItem->setData(0, DATA_ROLE_TYPE, ITEM_TYPE_GROUP);

    QFont font = groupItem->font(0);
    font.setBold(true);
    groupItem->setFont(0, font);

    if (parent) {
        parent->addChild(groupItem);
    } else {
        treeWidget_->addTopLevelItem(groupItem);
    }

    groupItems_[group.id] = groupItem;

    // Add child groups recursively
    auto childGroups = groupVm.getChildGroups(group.id);
    for (const auto& childGroup : childGroups) {
        addGroupToTree(childGroup, groupItem);
    }

    // Add hosts in this group
    auto hosts = groupVm.getHostsInGroup(group.id);
    for (const auto& host : hosts) {
        auto* hostItem = createHostItem(host);
        groupItem->addChild(hostItem);
    }
}

QTreeWidgetItem* HostListWidget::createHostItem(const core::Host& host) {
    auto* item = new QTreeWidgetItem();
    item->setData(COLUMN_HOST, DATA_ROLE_ID, QVariant::fromValue(host.id));
    item->setData(COLUMN_HOST, DATA_ROLE_TYPE, ITEM_TYPE_HOST);

    updateHostItemStatus(item, host);
    hostItems_[host.id] = item;

    // Create sparkline widget for this host
    auto* sparkline = new SparklineWidget();
    sparkline->setWarningThreshold(host.warningThresholdMs);
    sparkline->setCriticalThreshold(host.criticalThresholdMs);
    sparkline->setHostStatus(host.status);
    sparklines_[host.id] = sparkline;

    // Defer setting item widget until after item is added to tree
    QTimer::singleShot(0, this, [this, item, sparkline, hostId = host.id, host]() {
        if (treeWidget_ && hostItems_.count(hostId) > 0) {
            treeWidget_->setItemWidget(item, COLUMN_SPARKLINE, sparkline);
            initializeSparkline(hostId, sparkline, host);
        }
    });

    return item;
}

void HostListWidget::updateHostItemStatus(QTreeWidgetItem* item, const core::Host& host) {
    QString statusIcon;

    switch (host.status) {
    case core::HostStatus::Up:
        statusIcon = "🟢";
        break;
    case core::HostStatus::Warning:
        statusIcon = "🟡";
        break;
    case core::HostStatus::Down:
        statusIcon = "🔴";
        break;
    default:
        statusIcon = "⚪";
        break;
    }

    QString text = QString("%1 %2 (%3)")
                       .arg(statusIcon)
                       .arg(QString::fromStdString(host.name))
                       .arg(QString::fromStdString(host.address));

    item->setText(0, text);
    item->setToolTip(0, QString("Status: %1\nAddress: %2\nInterval: %3s")
                            .arg(QString::fromStdString(host.statusToString()))
                            .arg(QString::fromStdString(host.address))
                            .arg(host.pingIntervalSeconds));
}

void HostListWidget::updateHostStatus(int64_t hostId) {
    auto it = hostItems_.find(hostId);
    if (it == hostItems_.end()) {
        return;
    }

    auto& vm = app::Application::instance().hostMonitorViewModel();
    auto host = vm.getHost(hostId);

    if (host) {
        updateHostItemStatus(it->second, *host);

        // Update sparkline status color
        auto sparklineIt = sparklines_.find(hostId);
        if (sparklineIt != sparklines_.end()) {
            sparklineIt->second->setHostStatus(host->status);
        }
    }
}

void HostListWidget::updateHostSparkline(int64_t hostId, const core::PingResult& result) {
    auto it = sparklines_.find(hostId);
    if (it == sparklines_.end()) {
        return;
    }

    it->second->addDataPoint(result.latencyMs(), result.success);
}

void HostListWidget::initializeSparkline(int64_t hostId, SparklineWidget* sparkline,
                                          const core::Host& host) {
    auto& dashboardVm = app::Application::instance().dashboardViewModel();
    auto results = dashboardVm.getRecentResults(hostId, SPARKLINE_DATA_POINTS);

    // Results are returned newest first, so reverse for chronological order
    std::deque<double> latencies;
    std::deque<bool> successes;
    for (auto it = results.rbegin(); it != results.rend(); ++it) {
        latencies.push_back(it->latencyMs());
        successes.push_back(it->success);
    }

    sparkline->setData(latencies, successes);
    sparkline->setWarningThreshold(host.warningThresholdMs);
    sparkline->setCriticalThreshold(host.criticalThresholdMs);
    sparkline->setHostStatus(host.status);
}

int64_t HostListWidget::selectedHostId() const {
    auto items = treeWidget_->selectedItems();
    if (items.isEmpty()) {
        return -1;
    }

    auto* item = items.first();
    int type = item->data(0, DATA_ROLE_TYPE).toInt();

    if (type == ITEM_TYPE_HOST) {
        return item->data(0, DATA_ROLE_ID).toLongLong();
    }

    return -1;
}

void HostListWidget::onItemSelectionChanged() {
    auto items = treeWidget_->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    auto* item = items.first();
    int type = item->data(0, DATA_ROLE_TYPE).toInt();
    int64_t id = item->data(0, DATA_ROLE_ID).toLongLong();

    if (type == ITEM_TYPE_HOST) {
        emit hostSelected(id);
    } else if (type == ITEM_TYPE_GROUP) {
        emit groupSelected(id);
    }
}

void HostListWidget::onItemDoubleClicked(QTreeWidgetItem* item, int /*column*/) {
    if (!item) {
        return;
    }

    int type = item->data(0, DATA_ROLE_TYPE).toInt();

    if (type == ITEM_TYPE_HOST) {
        emit hostDoubleClicked(item->data(0, DATA_ROLE_ID).toLongLong());
    }
}

void HostListWidget::onContextMenuRequested(const QPoint& pos) {
    auto* item = treeWidget_->itemAt(pos);

    QMenu menu(this);

    if (!item) {
        // Context menu on empty space - would add "Add Group" option
        // but that would be handled at MainWindow level
        return;
    }

    int type = item->data(0, DATA_ROLE_TYPE).toInt();

    if (type == ITEM_TYPE_GROUP) {
        auto* deleteAction = menu.addAction("Delete Group");

        connect(deleteAction, &QAction::triggered, this, [this, item]() {
            int64_t groupId = item->data(0, DATA_ROLE_ID).toLongLong();
            auto& vm = app::Application::instance().hostGroupViewModel();
            vm.removeGroup(groupId);
            refreshHosts();
        });
    } else if (type == ITEM_TYPE_HOST) {
        auto* moveAction = menu.addMenu("Move to Group");
        auto& groupVm = app::Application::instance().hostGroupViewModel();
        auto groups = groupVm.getAllGroups();

        auto* ungroupAction = moveAction->addAction("(Ungrouped)");
        connect(ungroupAction, &QAction::triggered, this, [this, item]() {
            int64_t hostId = item->data(0, DATA_ROLE_ID).toLongLong();
            auto& vm = app::Application::instance().hostGroupViewModel();
            vm.assignHostToGroup(hostId, std::nullopt);
            refreshHosts();
        });

        if (!groups.empty()) {
            moveAction->addSeparator();
        }

        for (const auto& group : groups) {
            auto* action = moveAction->addAction(QString::fromStdString(group.name));
            connect(action, &QAction::triggered, this, [this, item, groupId = group.id]() {
                int64_t hostId = item->data(0, DATA_ROLE_ID).toLongLong();
                auto& vm = app::Application::instance().hostGroupViewModel();
                vm.assignHostToGroup(hostId, groupId);
                refreshHosts();
            });
        }
    }

    if (!menu.isEmpty()) {
        menu.exec(treeWidget_->mapToGlobal(pos));
    }
}

} // namespace netpulse::ui
