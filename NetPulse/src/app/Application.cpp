#include "app/Application.hpp"

#include "ui/windows/MainWindow.hpp"

#include <QStandardPaths>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace netpulse::app {

Application* Application::instance_ = nullptr;

Application::Application(int& argc, char** argv) {
    instance_ = this;

    qtApp_ = std::make_unique<QApplication>(argc, argv);
    qtApp_->setApplicationName("NetPulse");
    qtApp_->setApplicationVersion("1.0.0");
    qtApp_->setOrganizationName("NetPulse");

    initializeLogging();
    initializeComponents();
}

Application::~Application() {
    spdlog::info("Application shutting down...");

    if (restApiServer_) {
        restApiServer_->stop();
    }

    if (dashboardViewModel_) {
        dashboardViewModel_->stopMonitoring();
    }

    if (asioContext_) {
        asioContext_->stop();
    }

    performCleanup();
    instance_ = nullptr;
}

void Application::initializeLogging() {
    auto configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
    std::filesystem::create_directories(configDir);

    auto logPath = std::filesystem::path(configDir) / "netpulse.log";

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(spdlog::level::info);

    auto fileSink =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logPath.string(), 5 * 1024 * 1024, 3);
    fileSink->set_level(spdlog::level::debug);

    auto logger =
        std::make_shared<spdlog::logger>("netpulse", spdlog::sinks_init_list{consoleSink, fileSink});
    logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);

    spdlog::info("NetPulse {} starting...", qtApp_->applicationVersion().toStdString());
    spdlog::info("Log file: {}", logPath.string());
}

void Application::initializeComponents() {
    // Configuration
    auto configDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();
    config_ = std::make_unique<infra::ConfigManager>(configDir);
    config_->load();

    // Database
    database_ = std::make_shared<infra::Database>(config_->databasePath().string());
    database_->runMigrations();

    // Asio context
    asioContext_ = std::make_unique<infra::AsioContext>(4);
    asioContext_->start();

    // Network services
    pingService_ = std::make_shared<infra::PingService>(*asioContext_);
    portScanner_ = std::make_unique<infra::PortScanner>(*asioContext_);

    // Notification service
    notificationService_ = std::make_shared<infra::NotificationService>(database_);
    notificationService_->loadWebhooksFromDatabase();
    notificationService_->setEnabled(config_->config().webhooksEnabled);

    // ViewModels
    dashboardViewModel_ =
        std::make_unique<viewmodels::DashboardViewModel>(database_, pingService_);
    hostMonitorViewModel_ =
        std::make_unique<viewmodels::HostMonitorViewModel>(database_, pingService_);
    hostGroupViewModel_ = std::make_unique<viewmodels::HostGroupViewModel>(database_);
    alertsViewModel_ = std::make_unique<viewmodels::AlertsViewModel>(database_, notificationService_);

    // Configure alert thresholds
    alertsViewModel_->setThresholds(config_->config().alertThresholds);

    // REST API server
    if (config_->config().restApiEnabled) {
        restApiServer_ = std::make_shared<infra::RestApiServer>(
            *asioContext_, database_, config_->config().restApiPort);

        // Load API key from secure storage
        auto apiKey = config_->getSecureValue("rest_api_key");
        if (apiKey) {
            restApiServer_->setApiKey(*apiKey);
        }

        restApiServer_->start();
    }

    spdlog::info("Application components initialized");
}

void Application::performCleanup() {
    if (!config_->config().autoCleanup) {
        return;
    }

    auto maxAge = std::chrono::hours(config_->config().dataRetentionDays * 24);
    infra::MetricsRepository metricsRepo(database_);
    metricsRepo.cleanupOldPingResults(maxAge);
    metricsRepo.cleanupOldAlerts(maxAge);
    metricsRepo.cleanupOldPortScans(maxAge);

    spdlog::info("Performed data cleanup");
}

int Application::run() {
    // Create and show main window
    ui::MainWindow mainWindow;

    if (config_->config().windowMaximized) {
        mainWindow.showMaximized();
    } else {
        mainWindow.setGeometry(config_->config().windowX, config_->config().windowY,
                               config_->config().windowWidth, config_->config().windowHeight);

        if (config_->config().startMinimized) {
            mainWindow.hide();
        } else {
            mainWindow.show();
        }
    }

    // Start monitoring
    dashboardViewModel_->startMonitoring();

    return qtApp_->exec();
}

Application& Application::instance() {
    return *instance_;
}

} // namespace netpulse::app
