#pragma once

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

#include <functional>
#include <map>
#include <memory>
#include <string>

namespace netpulse::infra {

/**
 * @brief Response data from an HTTP request.
 */
struct HttpResponse {
    int statusCode{0};       ///< HTTP status code (e.g., 200, 404).
    std::string body;        ///< Response body content.
    std::string errorMessage; ///< Error message if request failed.
    bool success{false};     ///< True if request completed successfully.
};

/**
 * @brief Callback type for async HTTP request completion.
 */
using HttpCallback = std::function<void(const HttpResponse&)>;

/**
 * @brief Asynchronous HTTP client using Qt's network stack.
 *
 * Provides non-blocking HTTP POST requests with callback-based
 * response handling. Built on QNetworkAccessManager.
 */
class HttpClient : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs an HttpClient.
     * @param parent Optional parent QObject for memory management.
     */
    explicit HttpClient(QObject* parent = nullptr);

    /**
     * @brief Default destructor.
     */
    ~HttpClient() override = default;

    /**
     * @brief Performs an asynchronous HTTP POST request.
     * @param url Target URL for the request.
     * @param payload Request body content.
     * @param headers HTTP headers to include in the request.
     * @param timeoutMs Request timeout in milliseconds.
     * @param callback Function called when the request completes.
     */
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
