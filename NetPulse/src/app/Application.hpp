#pragma once

#include "infrastructure/config/ConfigManager.hpp"
#include "infrastructure/database/Database.hpp"
#include "infrastructure/network/AsioContext.hpp"
#include "infrastructure/network/PingService.hpp"
#include "infrastructure/network/PortScanner.hpp"
#include "infrastructure/notifications/NotificationService.hpp"
#include "viewmodels/AlertsViewModel.hpp"
#include "viewmodels/DashboardViewModel.hpp"
#include "viewmodels/HostGroupViewModel.hpp"
#include "viewmodels/HostMonitorViewModel.hpp"

#include <QApplication>
#include <memory>

namespace netpulse::app {

class Application {
public:
    Application(int& argc, char** argv);
    ~Application();

    int run();

    // Accessors
    infra::ConfigManager& config() { return *config_; }
    infra::Database& database() { return *database_; }
    infra::AsioContext& asioContext() { return *asioContext_; }
    infra::PingService& pingService() { return *pingService_; }
    infra::PortScanner& portScanner() { return *portScanner_; }

    viewmodels::DashboardViewModel& dashboardViewModel() { return *dashboardViewModel_; }
    viewmodels::HostMonitorViewModel& hostMonitorViewModel() { return *hostMonitorViewModel_; }
    viewmodels::HostGroupViewModel& hostGroupViewModel() { return *hostGroupViewModel_; }
    viewmodels::AlertsViewModel& alertsViewModel() { return *alertsViewModel_; }
    infra::NotificationService& notificationService() { return *notificationService_; }

    static Application& instance();

private:
    void initializeLogging();
    void initializeComponents();
    void performCleanup();

    std::unique_ptr<QApplication> qtApp_;
    std::unique_ptr<infra::ConfigManager> config_;
    std::shared_ptr<infra::Database> database_;
    std::unique_ptr<infra::AsioContext> asioContext_;
    std::shared_ptr<infra::PingService> pingService_;
    std::unique_ptr<infra::PortScanner> portScanner_;

    std::unique_ptr<viewmodels::DashboardViewModel> dashboardViewModel_;
    std::unique_ptr<viewmodels::HostMonitorViewModel> hostMonitorViewModel_;
    std::unique_ptr<viewmodels::HostGroupViewModel> hostGroupViewModel_;
    std::unique_ptr<viewmodels::AlertsViewModel> alertsViewModel_;
    std::shared_ptr<infra::NotificationService> notificationService_;

    static Application* instance_;
};

} // namespace netpulse::app
