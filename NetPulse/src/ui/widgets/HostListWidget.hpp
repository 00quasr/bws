#pragma once

#include "core/types/Host.hpp"
#include "ui/widgets/StatusIndicator.hpp"

#include <QListWidget>
#include <QWidget>
#include <map>

namespace netpulse::ui {

class HostListWidget : public QWidget {
    Q_OBJECT

public:
    explicit HostListWidget(QWidget* parent = nullptr);

    void refreshHosts();
    void updateHostStatus(int64_t hostId);

    int64_t selectedHostId() const;

signals:
    void hostSelected(int64_t hostId);
    void hostDoubleClicked(int64_t hostId);

private slots:
    void onItemSelectionChanged();
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void setupUi();
    QWidget* createHostItemWidget(const core::Host& host);

    QListWidget* listWidget_{nullptr};
    std::map<int64_t, QListWidgetItem*> hostItems_;
    std::map<int64_t, StatusIndicator*> statusIndicators_;
};

} // namespace netpulse::ui
