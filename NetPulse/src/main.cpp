#include "app/Application.hpp"
#include "app/SingleInstance.hpp"

#include <QMessageBox>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[]) {
    // Single instance check
    netpulse::app::SingleInstance singleInstance("NetPulse-SingleInstance");

    if (!singleInstance.tryLock()) {
        // Another instance is running, send activation message
        singleInstance.sendMessage("activate");
        return 0;
    }

    try {
        netpulse::app::Application app(argc, argv);

        // Handle messages from other instances
        QObject::connect(&singleInstance, &netpulse::app::SingleInstance::anotherInstanceStarted,
                         []() {
                             // Bring main window to front
                             // This would be handled by connecting to MainWindow
                             spdlog::info("Another instance tried to start");
                         });

        return app.run();
    } catch (const std::exception& e) {
        spdlog::critical("Fatal error: {}", e.what());
        // Only show message box if QApplication exists
        if (QCoreApplication::instance()) {
            QMessageBox::critical(nullptr, "NetPulse Error",
                                  QString("A fatal error occurred:\n%1").arg(e.what()));
        }
        return 1;
    }
}
