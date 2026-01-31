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

class Statement {
public:
    explicit Statement(sqlite3_stmt* stmt);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&& other) noexcept;
    Statement& operator=(Statement&& other) noexcept;

    void bind(int index, int value);
    void bind(int index, int64_t value);
    void bind(int index, double value);
    void bind(int index, const std::string& value);
    void bindNull(int index);

    bool step();
    void reset();

    int columnInt(int index) const;
    int64_t columnInt64(int index) const;
    double columnDouble(int index) const;
    std::string columnText(int index) const;
    bool columnIsNull(int index) const;

    sqlite3_stmt* handle() const { return stmt_; }

private:
    sqlite3_stmt* stmt_{nullptr};
};

class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    void execute(const std::string& sql);
    Statement prepare(const std::string& sql);

    int64_t lastInsertRowId() const;
    int changes() const;

    void beginTransaction();
    void commit();
    void rollback();

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

    void runMigrations();

    // Convenience variadic execute with binding
    template <typename... Args>
    void execute(const std::string& sql, Args&&... args) {
        auto stmt = prepare(sql);
        bindAll(stmt, 1, std::forward<Args>(args)...);
        stmt.step();
    }

    // Query that returns JSON-like rows
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
