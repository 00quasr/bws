#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace netpulse::infra {

struct HttpResponse {
    int statusCode{0};
    std::string body;
    std::string errorMessage;
    bool success{false};
};

using HttpCallback = std::function<void(const HttpResponse&)>;

class HttpClient : public QObject {
    Q_OBJECT

public:
    explicit HttpClient(QObject* parent = nullptr);
    ~HttpClient() override = default;

    void postAsync(const std::string& url, const std::string& payload,
                   const std::map<std::string, std::string>& headers, int timeoutMs,
                   HttpCallback callback);

private slots:
    void onRequestFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager manager_;
    std::map<QNetworkReply*, HttpCallback> pendingCallbacks_;
};

} // namespace netpulse::infra
