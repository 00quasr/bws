#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace netpulse::infra {

/**
 * @brief RAII wrapper for SQLite prepared statements.
 *
 * Provides type-safe parameter binding and column value extraction
 * for SQLite prepared statements. Supports move semantics.
 *
 * @note This class is non-copyable but moveable.
 */
class Statement {
public:
    /**
     * @brief Constructs a Statement from a raw SQLite statement handle.
     * @param stmt SQLite prepared statement handle (takes ownership).
     */
    explicit Statement(sqlite3_stmt* stmt);

    /**
     * @brief Destructor. Finalizes the statement.
     */
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    /**
     * @brief Move constructor.
     * @param other Statement to move from.
     */
    Statement(Statement&& other) noexcept;

    /**
     * @brief Move assignment operator.
     * @param other Statement to move from.
     * @return Reference to this Statement.
     */
    Statement& operator=(Statement&& other) noexcept;

    /**
     * @brief Binds an integer value to a parameter.
     * @param index Parameter index (1-based).
     * @param value Integer value to bind.
     */
    void bind(int index, int value);

    /**
     * @brief Binds a 64-bit integer value to a parameter.
     * @param index Parameter index (1-based).
     * @param value 64-bit integer value to bind.
     */
    void bind(int index, int64_t value);

    /**
     * @brief Binds a double value to a parameter.
     * @param index Parameter index (1-based).
     * @param value Double value to bind.
     */
    void bind(int index, double value);

    /**
     * @brief Binds a string value to a parameter.
     * @param index Parameter index (1-based).
     * @param value String value to bind.
     */
    void bind(int index, const std::string& value);

    /**
     * @brief Binds NULL to a parameter.
     * @param index Parameter index (1-based).
     */
    void bindNull(int index);

    /**
     * @brief Executes the statement and advances to the next row.
     * @return True if a row is available, false if done.
     */
    bool step();

    /**
     * @brief Resets the statement for re-execution.
     */
    void reset();

    /**
     * @brief Retrieves an integer column value.
     * @param index Column index (0-based).
     * @return Integer value.
     */
    int columnInt(int index) const;

    /**
     * @brief Retrieves a 64-bit integer column value.
     * @param index Column index (0-based).
     * @return 64-bit integer value.
     */
    int64_t columnInt64(int index) const;

    /**
     * @brief Retrieves a double column value.
     * @param index Column index (0-based).
     * @return Double value.
     */
    double columnDouble(int index) const;

    /**
     * @brief Retrieves a text column value.
     * @param index Column index (0-based).
     * @return String value.
     */
    std::string columnText(int index) const;

    /**
     * @brief Checks if a column value is NULL.
     * @param index Column index (0-based).
     * @return True if NULL, false otherwise.
     */
    bool columnIsNull(int index) const;

    /**
     * @brief Returns the raw SQLite statement handle.
     * @return Pointer to the sqlite3_stmt.
     */
    sqlite3_stmt* handle() const { return stmt_; }

private:
    sqlite3_stmt* stmt_{nullptr};
};

/**
 * @brief SQLite database wrapper with connection management.
 *
 * Provides high-level database operations including transactions,
 * prepared statements, and automatic schema migrations. Uses WAL
 * mode for concurrent access.
 *
 * @note This class is non-copyable.
 */
class Database {
public:
    /**
     * @brief Opens or creates a database at the specified path.
     * @param path File path to the SQLite database.
     * @throws std::runtime_error if database cannot be opened.
     */
    explicit Database(const std::string& path);

    /**
     * @brief Destructor. Closes the database connection.
     */
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    /**
     * @brief Executes a SQL statement without returning results.
     * @param sql SQL statement to execute.
     * @throws std::runtime_error on SQL error.
     */
    void execute(const std::string& sql);

    /**
     * @brief Prepares a SQL statement for execution.
     * @param sql SQL statement to prepare.
     * @return Prepared Statement object.
     * @throws std::runtime_error if preparation fails.
     */
    Statement prepare(const std::string& sql);

    /**
     * @brief Returns the row ID of the last inserted row.
     * @return Last insert row ID.
     */
    int64_t lastInsertRowId() const;

    /**
     * @brief Returns the number of rows affected by the last statement.
     * @return Number of changed rows.
     */
    int changes() const;

    /**
     * @brief Begins a database transaction.
     */
    void beginTransaction();

    /**
     * @brief Commits the current transaction.
     */
    void commit();

    /**
     * @brief Rolls back the current transaction.
     */
    void rollback();

    /**
     * @brief Executes a function within a transaction.
     *
     * Automatically commits on success or rolls back on exception.
     *
     * @tparam Func Callable type.
     * @param func Function to execute within the transaction.
     */
    template <typename Func>
    void transaction(Func&& func) {
        beginTransaction();
        try {
            func();
            commit();
        } catch (...) {
            rollback();
            throw;
        }
    }

    /**
     * @brief Runs pending database schema migrations.
     */
    void runMigrations();

    /**
     * @brief Executes a SQL statement with bound parameters.
     * @tparam Args Parameter types.
     * @param sql SQL statement with placeholders.
     * @param args Values to bind to placeholders.
     */
    template <typename... Args>
    void execute(const std::string& sql, Args&&... args) {
        auto stmt = prepare(sql);
        bindAll(stmt, 1, std::forward<Args>(args)...);
        stmt.step();
    }

    /**
     * @brief Executes a query and returns results as JSON objects.
     * @tparam Args Parameter types.
     * @param sql SQL query with placeholders.
     * @param args Values to bind to placeholders.
     * @return Vector of JSON objects, one per row.
     */
    template <typename... Args>
    std::vector<nlohmann::json> query(const std::string& sql, Args&&... args) {
        std::vector<nlohmann::json> results;
        auto stmt = prepare(sql);
        if constexpr (sizeof...(args) > 0) {
            bindAll(stmt, 1, std::forward<Args>(args)...);
        }

        std::lock_guard lock(mutex_);
        sqlite3_stmt* rawStmt = getStmtHandle(stmt);
        int columnCount = sqlite3_column_count(rawStmt);

        while (stmt.step()) {
            nlohmann::json row;
            for (int i = 0; i < columnCount; ++i) {
                const char* name = sqlite3_column_name(rawStmt, i);
                int type = sqlite3_column_type(rawStmt, i);

                switch (type) {
                case SQLITE_INTEGER:
                    row[name] = stmt.columnInt64(i);
                    break;
                case SQLITE_FLOAT:
                    row[name] = stmt.columnDouble(i);
                    break;
                case SQLITE_TEXT:
                    row[name] = stmt.columnText(i);
                    break;
                case SQLITE_NULL:
                    row[name] = nullptr;
                    break;
                default:
                    row[name] = stmt.columnText(i);
                    break;
                }
            }
            results.push_back(row);
        }
        return results;
    }

private:
    template <typename T, typename... Rest>
    void bindAll(Statement& stmt, int index, T&& first, Rest&&... rest) {
        bindValue(stmt, index, std::forward<T>(first));
        if constexpr (sizeof...(rest) > 0) {
            bindAll(stmt, index + 1, std::forward<Rest>(rest)...);
        }
    }

    void bindValue(Statement& stmt, int index, int value) { stmt.bind(index, value); }
    void bindValue(Statement& stmt, int index, int64_t value) { stmt.bind(index, value); }
    void bindValue(Statement& stmt, int index, double value) { stmt.bind(index, value); }
    void bindValue(Statement& stmt, int index, const std::string& value) { stmt.bind(index, value); }
    void bindValue(Statement& stmt, int index, const char* value) { stmt.bind(index, std::string(value)); }

    sqlite3_stmt* getStmtHandle(Statement& stmt) const;

private:
    void enableWAL();
    void createMigrationsTable();
    int getCurrentVersion();
    void setVersion(int version);

    sqlite3* db_{nullptr};
    mutable std::mutex mutex_;
};

} // namespace netpulse::infra
