#pragma once

#include <functional>
#include <memory>
#include <mutex>
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

private:
    void enableWAL();
    void createMigrationsTable();
    int getCurrentVersion();
    void setVersion(int version);

    sqlite3* db_{nullptr};
    mutable std::mutex mutex_;
};

} // namespace netpulse::infra
