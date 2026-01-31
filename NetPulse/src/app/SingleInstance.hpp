#pragma once

#include <QLocalServer>
#include <QLocalSocket>
#include <QObject>
#include <memory>

namespace netpulse::app {

class SingleInstance : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(const QString& key, QObject* parent = nullptr);
    ~SingleInstance() override;

    bool tryLock();
    bool isRunning() const { return isRunning_; }

    void sendMessage(const QString& message);

signals:
    void messageReceived(const QString& message);
    void anotherInstanceStarted();

private slots:
    void onNewConnection();

private:
    QString key_;
    bool isRunning_{false};
    std::unique_ptr<QLocalServer> server_;
};

} // namespace netpulse::app
