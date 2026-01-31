#include "app/SingleInstance.hpp"

#include <QDataStream>
#include <spdlog/spdlog.h>

namespace netpulse::app {

SingleInstance::SingleInstance(const QString& key, QObject* parent)
    : QObject(parent), key_(key) {}

SingleInstance::~SingleInstance() {
    if (server_) {
        server_->close();
    }
}

bool SingleInstance::tryLock() {
    // Try to connect to existing instance
    QLocalSocket socket;
    socket.connectToServer(key_);

    if (socket.waitForConnected(500)) {
        // Another instance is running
        isRunning_ = true;
        spdlog::info("Another instance detected");
        return false;
    }

    // No other instance, start server
    QLocalServer::removeServer(key_);
    server_ = std::make_unique<QLocalServer>(this);

    if (!server_->listen(key_)) {
        spdlog::error("Failed to start single instance server: {}",
                      server_->errorString().toStdString());
        return false;
    }

    connect(server_.get(), &QLocalServer::newConnection, this, &SingleInstance::onNewConnection);

    spdlog::debug("Single instance server started");
    return true;
}

void SingleInstance::sendMessage(const QString& message) {
    QLocalSocket socket;
    socket.connectToServer(key_);

    if (!socket.waitForConnected(1000)) {
        spdlog::warn("Failed to connect to primary instance");
        return;
    }

    QDataStream stream(&socket);
    stream << message;
    socket.flush();
    socket.waitForBytesWritten(1000);
}

void SingleInstance::onNewConnection() {
    auto* socket = server_->nextPendingConnection();
    if (!socket) {
        return;
    }

    connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
        QDataStream stream(socket);
        QString message;
        stream >> message;

        if (!message.isEmpty()) {
            emit messageReceived(message);
        }

        emit anotherInstanceStarted();
        socket->deleteLater();
    });

    connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
}

} // namespace netpulse::app
