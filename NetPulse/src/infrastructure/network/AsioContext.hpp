#pragma once

#include <asio.hpp>
#include <atomic>
#include <optional>
#include <thread>
#include <vector>

namespace netpulse::infra {

/**
 * @brief Manages an Asio I/O context with a thread pool for async operations.
 *
 * Provides a wrapper around asio::io_context that manages a pool of worker threads
 * for executing asynchronous I/O operations. Uses executor_work_guard to keep
 * the context running until explicitly stopped.
 *
 * @note This class is non-copyable. Use the singleton instance() for shared access.
 */
class AsioContext {
public:
    /**
     * @brief Constructs an AsioContext with the specified number of threads.
     * @param threadCount Number of worker threads (defaults to hardware concurrency).
     */
    explicit AsioContext(size_t threadCount = std::thread::hardware_concurrency());

    /**
     * @brief Destructor. Stops the context and joins all threads.
     */
    ~AsioContext();

    AsioContext(const AsioContext&) = delete;
    AsioContext& operator=(const AsioContext&) = delete;

    /**
     * @brief Starts the I/O context and worker threads.
     *
     * Creates the work guard and spawns worker threads to process async operations.
     * Has no effect if already running.
     */
    void start();

    /**
     * @brief Stops the I/O context and joins all worker threads.
     *
     * Releases the work guard, stops the io_context, and waits for all threads to finish.
     */
    void stop();

    /**
     * @brief Returns a reference to the underlying Asio io_context.
     * @return Reference to the asio::io_context.
     */
    asio::io_context& getContext() { return ioContext_; }

    /**
     * @brief Posts a handler to be executed asynchronously.
     * @tparam Handler Callable type (function, lambda, etc.).
     * @param handler The handler to execute on the I/O thread pool.
     */
    template <typename Handler>
    void post(Handler&& handler) {
        asio::post(ioContext_, std::forward<Handler>(handler));
    }

    /**
     * @brief Returns the global singleton instance.
     * @return Reference to the shared AsioContext instance.
     */
    static AsioContext& instance();

private:
    using WorkGuard = asio::executor_work_guard<asio::io_context::executor_type>;

    asio::io_context ioContext_;
    std::optional<WorkGuard> workGuard_;
    std::vector<std::thread> threads_;
    std::atomic<bool> running_{false};
    size_t threadCount_;
};

} // namespace netpulse::infra
