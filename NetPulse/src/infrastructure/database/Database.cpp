#include "infrastructure/database/Database.hpp"

#include <spdlog/spdlog.h>
#include <stdexcept>

namespace netpulse::infra {

// Statement implementation
Statement::Statement(sqlite3_stmt* stmt) : stmt_(stmt) {}

Statement::~Statement() {
    if (stmt_) {
        sqlite3_finalize(stmt_);
    }
}

Statement::Statement(Statement&& other) noexcept : stmt_(other.stmt_) {
    other.stmt_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (stmt_) {
            sqlite3_finalize(stmt_);
        }
        stmt_ = other.stmt_;
        other.stmt_ = nullptr;
    }
    return *this;
}

void Statement::bind(int index, int value) {
    if (sqlite3_bind_int(stmt_, index, value) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind int parameter");
    }
}

void Statement::bind(int index, int64_t value) {
    if (sqlite3_bind_int64(stmt_, index, value) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind int64 parameter");
    }
}

void Statement::bind(int index, double value) {
    if (sqlite3_bind_double(stmt_, index, value) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind double parameter");
    }
}

void Statement::bind(int index, const std::string& value) {
    if (sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()),
                          SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind text parameter");
    }
}

void Statement::bindNull(int index) {
    if (sqlite3_bind_null(stmt_, index) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind null parameter");
    }
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    }
    if (rc == SQLITE_DONE) {
        return false;
    }
    throw std::runtime_error(std::string("SQLite step failed: ") + sqlite3_errstr(rc));
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

int Statement::columnInt(int index) const {
    return sqlite3_column_int(stmt_, index);
}

int64_t Statement::columnInt64(int index) const {
    return sqlite3_column_int64(stmt_, index);
}

double Statement::columnDouble(int index) const {
    return sqlite3_column_double(stmt_, index);
}

std::string Statement::columnText(int index) const {
    const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, index));
    return text ? text : "";
}

bool Statement::columnIsNull(int index) const {
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

// Database implementation
Database::Database(const std::string& path) {
    spdlog::info("Opening database: {}", path);

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        std::string error = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        throw std::runtime_error("Failed to open database: " + error);
    }

    enableWAL();
    createMigrationsTable();
}

Database::~Database() {
    if (db_) {
        sqlite3_close(db_);
    }
}

void Database::enableWAL() {
    execute("PRAGMA journal_mode=WAL");
    execute("PRAGMA synchronous=NORMAL");
    execute("PRAGMA foreign_keys=ON");
}

void Database::execute(const std::string& sql) {
    std::lock_guard lock(mutex_);
    char* errMsg = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::string error = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SQL execution failed: " + error);
    }
}

Statement Database::prepare(const std::string& sql) {
    std::lock_guard lock(mutex_);
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to prepare statement: ") +
                                 sqlite3_errmsg(db_));
    }
    return Statement(stmt);
}

int64_t Database::lastInsertRowId() const {
    return sqlite3_last_insert_rowid(db_);
}

int Database::changes() const {
    return sqlite3_changes(db_);
}

void Database::beginTransaction() {
    execute("BEGIN TRANSACTION");
}

void Database::commit() {
    execute("COMMIT");
}

void Database::rollback() {
    execute("ROLLBACK");
}

void Database::createMigrationsTable() {
    execute(R"(
        CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    )");
}

int Database::getCurrentVersion() {
    auto stmt = prepare("SELECT MAX(version) FROM schema_migrations");
    if (stmt.step() && !stmt.columnIsNull(0)) {
        return stmt.columnInt(0);
    }
    return 0;
}

void Database::setVersion(int version) {
    auto stmt = prepare("INSERT INTO schema_migrations (version) VALUES (?)");
    stmt.bind(1, version);
    stmt.step();
}

void Database::runMigrations() {
    spdlog::info("Running database migrations...");

    int currentVersion = getCurrentVersion();
    spdlog::info("Current schema version: {}", currentVersion);

    // Migration 1: Initial schema
    if (currentVersion < 1) {
        spdlog::info("Applying migration 1: Initial schema");
        execute(R"(
            CREATE TABLE IF NOT EXISTS hosts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                address TEXT NOT NULL UNIQUE,
                ping_interval INTEGER DEFAULT 30,
                warning_threshold_ms INTEGER DEFAULT 100,
                critical_threshold_ms INTEGER DEFAULT 500,
                status INTEGER DEFAULT 0,
                enabled INTEGER DEFAULT 1,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                last_checked TEXT
            )
        )");

        execute(R"(
            CREATE TABLE IF NOT EXISTS ping_results (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_id INTEGER NOT NULL REFERENCES hosts(id) ON DELETE CASCADE,
                timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
                latency_us INTEGER,
                success INTEGER,
                ttl INTEGER
            )
        )");

        execute("CREATE INDEX IF NOT EXISTS idx_ping_results_host_id ON ping_results(host_id)");
        execute(
            "CREATE INDEX IF NOT EXISTS idx_ping_results_timestamp ON ping_results(timestamp)");

        execute(R"(
            CREATE TABLE IF NOT EXISTS alerts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                host_id INTEGER,
                alert_type INTEGER,
                severity INTEGER,
                title TEXT,
                message TEXT,
                timestamp TEXT DEFAULT CURRENT_TIMESTAMP,
                acknowledged INTEGER DEFAULT 0
            )
        )");

        execute("CREATE INDEX IF NOT EXISTS idx_alerts_timestamp ON alerts(timestamp)");

        execute(R"(
            CREATE TABLE IF NOT EXISTS port_scan_results (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                target_address TEXT NOT NULL,
                port INTEGER NOT NULL,
                state INTEGER,
                service_name TEXT,
                scan_timestamp TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )");

        execute("CREATE INDEX IF NOT EXISTS idx_port_scan_address ON port_scan_results(target_address)");

        setVersion(1);
    }

    // Migration 2: Add host groups
    if (currentVersion < 2) {
        spdlog::info("Applying migration 2: Add host groups");
        execute(R"(
            CREATE TABLE IF NOT EXISTS host_groups (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT NOT NULL,
                description TEXT DEFAULT '',
                parent_id INTEGER REFERENCES host_groups(id) ON DELETE SET NULL,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        )");

        execute("CREATE INDEX IF NOT EXISTS idx_host_groups_parent ON host_groups(parent_id)");

        execute("ALTER TABLE hosts ADD COLUMN group_id INTEGER REFERENCES host_groups(id) ON DELETE SET NULL");

        setVersion(2);
    }

    spdlog::info("Database migrations complete. Version: {}", getCurrentVersion());
}

} // namespace netpulse::infra
