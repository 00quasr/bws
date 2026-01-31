#include "infrastructure/notifications/HttpClient.hpp"

#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <spdlog/spdlog.h>

namespace netpulse::infra {

HttpClient::HttpClient(QObject* parent) : QObject(parent) {
    connect(&manager_, &QNetworkAccessManager::finished, this, &HttpClient::onRequestFinished);
}

void HttpClient::postAsync(const std::string& url, const std::string& payload,
                           const std::map<std::string, std::string>& headers, int timeoutMs,
                           HttpCallback callback) {
    QNetworkRequest request(QUrl(QString::fromStdString(url)));

    for (const auto& [key, value] : headers) {
        request.setRawHeader(QByteArray::fromStdString(key), QByteArray::fromStdString(value));
    }

    request.setTransferTimeout(timeoutMs);

    QByteArray data = QByteArray::fromStdString(payload);
    QNetworkReply* reply = manager_.post(request, data);

    if (reply) {
        pendingCallbacks_[reply] = std::move(callback);
        spdlog::debug("HTTP POST request sent to: {}", url);
    } else {
        HttpResponse response;
        response.success = false;
        response.errorMessage = "Failed to create network request";
        callback(response);
    }
}

void HttpClient::onRequestFinished(QNetworkReply* reply) {
    auto it = pendingCallbacks_.find(reply);
    if (it == pendingCallbacks_.end()) {
        reply->deleteLater();
        return;
    }

    HttpCallback callback = std::move(it->second);
    pendingCallbacks_.erase(it);

    HttpResponse response;
    response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.body = reply->readAll().toStdString();

    if (reply->error() == QNetworkReply::NoError) {
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
        if (!response.success) {
            response.errorMessage = "HTTP error: " + std::to_string(response.statusCode);
        }
    } else {
        response.success = false;
        response.errorMessage = reply->errorString().toStdString();
        spdlog::warn("HTTP request failed: {} (status: {})", response.errorMessage,
                     response.statusCode);
    }

    reply->deleteLater();
    callback(response);
}

} // namespace netpulse::infra
