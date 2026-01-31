#pragma once

#include "core/types/Host.hpp"
#include "core/types/HostGroup.hpp"
#include "core/types/PingResult.hpp"
#include "ui/widgets/SparklineWidget.hpp"
#include "ui/widgets/StatusIndicator.hpp"

#include <QTreeWidget>
#include <QWidget>
#include <map>

namespace netpulse::ui {

class HostListWidget : public QWidget {
    Q_OBJECT

public:
    explicit HostListWidget(QWidget* parent = nullptr);

    void refreshHosts();
    void updateHostStatus(int64_t hostId);
    void updateHostSparkline(int64_t hostId, const core::PingResult& result);

    int64_t selectedHostId() const;

signals:
    void hostSelected(int64_t hostId);
    void hostDoubleClicked(int64_t hostId);
    void groupSelected(int64_t groupId);

private slots:
    void onItemSelectionChanged();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onContextMenuRequested(const QPoint& pos);

private:
    void setupUi();
    void populateTree();
    void addGroupToTree(const core::HostGroup& group, QTreeWidgetItem* parent = nullptr);
    QTreeWidgetItem* createHostItem(const core::Host& host);
    void updateHostItemStatus(QTreeWidgetItem* item, const core::Host& host);
    void initializeSparkline(int64_t hostId, SparklineWidget* sparkline, const core::Host& host);

    QTreeWidget* treeWidget_{nullptr};
    std::map<int64_t, QTreeWidgetItem*> hostItems_;
    std::map<int64_t, QTreeWidgetItem*> groupItems_;
    std::map<int64_t, StatusIndicator*> statusIndicators_;
    std::map<int64_t, SparklineWidget*> sparklines_;
};

} // namespace netpulse::ui
