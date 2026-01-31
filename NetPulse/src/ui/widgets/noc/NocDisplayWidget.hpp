#pragma once

#include "ui/widgets/noc/NocHostCard.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QTimer>
#include <QWidget>

namespace netpulse::ui {

class NocDisplayWidget : public QWidget {
    Q_OBJECT

public:
    explicit NocDisplayWidget(QWidget* parent = nullptr);
    ~NocDisplayWidget() override = default;

    void refresh();

signals:
    void exitRequested();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void setupUi();
    void setupHeader();
    void setupHostGrid();
    void setupFooter();
    void updateHostCards();
    void updateSummary();

    QLabel* titleLabel_{nullptr};
    QLabel* clockLabel_{nullptr};
    QLabel* summaryLabel_{nullptr};
    QLabel* uptimeLabel_{nullptr};

    QWidget* hostGridWidget_{nullptr};
    QGridLayout* hostGridLayout_{nullptr};
    std::vector<NocHostCard*> hostCards_;

    QTimer* refreshTimer_{nullptr};
    QTimer* clockTimer_{nullptr};

    static constexpr int COLUMNS = 4;
};

} // namespace netpulse::ui
