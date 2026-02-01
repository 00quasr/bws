#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include "infrastructure/database/Database.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

using namespace netpulse::infra;

namespace {

class BenchmarkDatabase {
public:
    BenchmarkDatabase()
        : dbPath_(std::filesystem::temp_directory_path() / "netpulse_bench_db.db") {
        std::filesystem::remove(dbPath_);
        db_ = std::make_shared<Database>(dbPath_.string());
        db_->runMigrations();
        createBenchmarkTables();
    }

    ~BenchmarkDatabase() {
        db_.reset();
        std::filesystem::remove(dbPath_);
    }

    std::shared_ptr<Database> get() { return db_; }

private:
    void createBenchmarkTables() {
        db_->execute(R"(
            CREATE TABLE IF NOT EXISTS bench_simple (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                int_val INTEGER,
                real_val REAL,
                text_val TEXT
            )
        )");

        db_->execute(R"(
            CREATE TABLE IF NOT EXISTS bench_indexed (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                category INTEGER,
                value REAL,
                name TEXT
            )
        )");
        db_->execute("CREATE INDEX IF NOT EXISTS idx_bench_category ON bench_indexed(category)");

        db_->execute(R"(
            CREATE TABLE IF NOT EXISTS bench_large_text (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                content TEXT
            )
        )");
    }

    std::filesystem::path dbPath_;
    std::shared_ptr<Database> db_;
};

} // namespace

// =============================================================================
// Statement Preparation Benchmarks
// =============================================================================

TEST_CASE("Database statement preparation benchmarks", "[benchmark][Database][prepare]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    BENCHMARK("Prepare simple SELECT") {
        return db->prepare("SELECT 1");
    };

    BENCHMARK("Prepare SELECT with WHERE") {
        return db->prepare("SELECT * FROM bench_simple WHERE id = ?");
    };

    BENCHMARK("Prepare INSERT") {
        return db->prepare("INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)");
    };

    BENCHMARK("Prepare complex SELECT with JOIN") {
        return db->prepare(R"(
            SELECT s.id, s.int_val, i.value
            FROM bench_simple s
            LEFT JOIN bench_indexed i ON s.id = i.id
            WHERE s.int_val > ?
            ORDER BY s.id DESC
            LIMIT ?
        )");
    };
}

// =============================================================================
// Statement Binding Benchmarks
// =============================================================================

TEST_CASE("Database statement binding benchmarks", "[benchmark][Database][bind]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    auto stmt = db->prepare("INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)");

    BENCHMARK("Bind int value") {
        stmt.reset();
        stmt.bind(1, 42);
        return 0;
    };

    BENCHMARK("Bind int64 value") {
        stmt.reset();
        stmt.bind(1, static_cast<int64_t>(9223372036854775807LL));
        return 0;
    };

    BENCHMARK("Bind double value") {
        stmt.reset();
        stmt.bind(2, 3.14159265358979);
        return 0;
    };

    BENCHMARK("Bind short string") {
        stmt.reset();
        stmt.bind(3, std::string("hello"));
        return 0;
    };

    BENCHMARK("Bind medium string (100 chars)") {
        stmt.reset();
        stmt.bind(3, std::string(100, 'x'));
        return 0;
    };

    BENCHMARK("Bind long string (1000 chars)") {
        stmt.reset();
        stmt.bind(3, std::string(1000, 'y'));
        return 0;
    };

    BENCHMARK("Bind all three types") {
        stmt.reset();
        stmt.bind(1, 42);
        stmt.bind(2, 3.14);
        stmt.bind(3, std::string("test value"));
        return 0;
    };

    BENCHMARK("Bind NULL value") {
        stmt.reset();
        stmt.bindNull(1);
        return 0;
    };
}

// =============================================================================
// Insert Benchmarks
// =============================================================================

TEST_CASE("Database insert benchmarks", "[benchmark][Database][insert]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    BENCHMARK("Insert single row (simple)") {
        db->execute("INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)",
                   42, 3.14, "test");
        return db->lastInsertRowId();
    };

    BENCHMARK("Insert single row (prepare + bind + step)") {
        auto stmt = db->prepare(
            "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)");
        stmt.bind(1, 42);
        stmt.bind(2, 3.14);
        stmt.bind(3, std::string("test"));
        stmt.step();
        return db->lastInsertRowId();
    };

    BENCHMARK("Insert with large text (1KB)") {
        std::string largeText(1024, 'A');
        db->execute("INSERT INTO bench_large_text (content) VALUES (?)", largeText);
        return db->lastInsertRowId();
    };

    BENCHMARK("Insert with large text (10KB)") {
        std::string largeText(10240, 'B');
        db->execute("INSERT INTO bench_large_text (content) VALUES (?)", largeText);
        return db->lastInsertRowId();
    };
}

// =============================================================================
// Query Benchmarks
// =============================================================================

TEST_CASE("Database query benchmarks", "[benchmark][Database][query]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    // Pre-populate data
    db->transaction([&] {
        for (int i = 0; i < 1000; ++i) {
            db->execute(
                "INSERT INTO bench_indexed (category, value, name) VALUES (?, ?, ?)",
                i % 10, static_cast<double>(i) * 1.5, "Item " + std::to_string(i));
        }
    });

    BENCHMARK("Query single row by primary key") {
        return db->query("SELECT * FROM bench_indexed WHERE id = ?", 500);
    };

    BENCHMARK("Query rows by indexed column") {
        return db->query("SELECT * FROM bench_indexed WHERE category = ?", 5);
    };

    BENCHMARK("Query with LIMIT 10") {
        return db->query("SELECT * FROM bench_indexed ORDER BY id DESC LIMIT 10");
    };

    BENCHMARK("Query with LIMIT 100") {
        return db->query("SELECT * FROM bench_indexed ORDER BY id DESC LIMIT 100");
    };

    BENCHMARK("Query with aggregate (COUNT)") {
        return db->query("SELECT category, COUNT(*) as cnt FROM bench_indexed GROUP BY category");
    };

    BENCHMARK("Query with aggregate (AVG, MIN, MAX)") {
        return db->query(
            "SELECT category, AVG(value), MIN(value), MAX(value) FROM bench_indexed GROUP BY category");
    };

    BENCHMARK("Query with LIKE pattern") {
        return db->query("SELECT * FROM bench_indexed WHERE name LIKE ? LIMIT 50", "%5%");
    };

    BENCHMARK("Query with range condition") {
        return db->query(
            "SELECT * FROM bench_indexed WHERE value BETWEEN ? AND ? LIMIT 100",
            100.0, 500.0);
    };
}

// =============================================================================
// Transaction Benchmarks
// =============================================================================

TEST_CASE("Database transaction benchmarks", "[benchmark][Database][transaction]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    BENCHMARK("Empty transaction") {
        db->transaction([&] {});
        return 0;
    };

    BENCHMARK("Transaction with 10 inserts") {
        db->transaction([&] {
            for (int i = 0; i < 10; ++i) {
                db->execute(
                    "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)",
                    i, static_cast<double>(i), "item");
            }
        });
        return 10;
    };

    BENCHMARK("Transaction with 100 inserts") {
        db->transaction([&] {
            for (int i = 0; i < 100; ++i) {
                db->execute(
                    "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)",
                    i, static_cast<double>(i), "item");
            }
        });
        return 100;
    };

    BENCHMARK("10 inserts without transaction") {
        for (int i = 0; i < 10; ++i) {
            db->execute(
                "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)",
                i, static_cast<double>(i), "item");
        }
        return 10;
    };

    BENCHMARK("beginTransaction + commit") {
        db->beginTransaction();
        db->commit();
        return 0;
    };
}

// =============================================================================
// Column Reading Benchmarks
// =============================================================================

TEST_CASE("Database column reading benchmarks", "[benchmark][Database][columns]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    // Insert test row
    db->execute(
        "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)",
        12345, 3.14159265358979, "This is a test string with some content");
    int64_t rowId = db->lastInsertRowId();

    auto stmt = db->prepare("SELECT id, int_val, real_val, text_val FROM bench_simple WHERE id = ?");
    stmt.bind(1, rowId);
    stmt.step();

    BENCHMARK("columnInt") {
        return stmt.columnInt(1);
    };

    BENCHMARK("columnInt64") {
        return stmt.columnInt64(0);
    };

    BENCHMARK("columnDouble") {
        return stmt.columnDouble(2);
    };

    BENCHMARK("columnText") {
        return stmt.columnText(3);
    };

    BENCHMARK("columnIsNull (not null)") {
        return stmt.columnIsNull(1);
    };

    BENCHMARK("Read all columns") {
        auto id = stmt.columnInt64(0);
        auto intVal = stmt.columnInt(1);
        auto realVal = stmt.columnDouble(2);
        auto textVal = stmt.columnText(3);
        return id + intVal + static_cast<int64_t>(realVal) + static_cast<int64_t>(textVal.size());
    };
}

// =============================================================================
// Batch Operation Benchmarks
// =============================================================================

TEST_CASE("Database batch operation benchmarks", "[benchmark][Database][batch]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    BENCHMARK_ADVANCED("Batch insert 100 rows (prepared statement reuse)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&db] {
            db->transaction([&] {
                auto stmt = db->prepare(
                    "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)");
                for (int i = 0; i < 100; ++i) {
                    stmt.reset();
                    stmt.bind(1, i);
                    stmt.bind(2, static_cast<double>(i) * 1.5);
                    stmt.bind(3, std::string("Item ") + std::to_string(i));
                    stmt.step();
                }
            });
            return 100;
        });
    };

    BENCHMARK_ADVANCED("Batch insert 100 rows (execute each time)")
    (Catch::Benchmark::Chronometer meter) {
        meter.measure([&db] {
            db->transaction([&] {
                for (int i = 0; i < 100; ++i) {
                    db->execute(
                        "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)",
                        i, static_cast<double>(i) * 1.5, "Item " + std::to_string(i));
                }
            });
            return 100;
        });
    };

    // Pre-populate for update benchmark
    db->transaction([&] {
        for (int i = 0; i < 500; ++i) {
            db->execute(
                "INSERT INTO bench_indexed (category, value, name) VALUES (?, ?, ?)",
                i % 10, static_cast<double>(i), "Name" + std::to_string(i));
        }
    });

    BENCHMARK("Batch update by category") {
        db->execute("UPDATE bench_indexed SET value = value + 1 WHERE category = ?", 5);
        return db->changes();
    };

    BENCHMARK("Batch delete by category") {
        // This will delete ~50 rows (category 9)
        db->execute("DELETE FROM bench_indexed WHERE category = ? AND id > 450", 9);
        return db->changes();
    };
}

// =============================================================================
// Execute vs Query Benchmarks
// =============================================================================

TEST_CASE("Database execute vs query benchmarks", "[benchmark][Database][compare]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    // Pre-populate
    db->transaction([&] {
        for (int i = 0; i < 100; ++i) {
            db->execute(
                "INSERT INTO bench_simple (int_val, real_val, text_val) VALUES (?, ?, ?)",
                i, static_cast<double>(i), "value");
        }
    });

    BENCHMARK("execute() for INSERT") {
        db->execute("INSERT INTO bench_simple (int_val) VALUES (?)", 42);
        return db->lastInsertRowId();
    };

    BENCHMARK("query() for SELECT (returns JSON)") {
        return db->query("SELECT * FROM bench_simple LIMIT 10");
    };

    BENCHMARK("prepare + step for SELECT (manual)") {
        auto stmt = db->prepare("SELECT * FROM bench_simple LIMIT 10");
        std::vector<int> ids;
        while (stmt.step()) {
            ids.push_back(stmt.columnInt(0));
        }
        return ids.size();
    };
}

// =============================================================================
// Stress Test Benchmarks
// =============================================================================

TEST_CASE("Database stress benchmarks", "[benchmark][Database][stress]") {
    BenchmarkDatabase testDb;
    auto db = testDb.get();

    BENCHMARK("1000 sequential inserts with transaction") {
        db->transaction([&] {
            for (int i = 0; i < 1000; ++i) {
                db->execute("INSERT INTO bench_simple (int_val) VALUES (?)", i);
            }
        });
        return 1000;
    };

    // Pre-populate for query stress test
    db->transaction([&] {
        for (int i = 0; i < 5000; ++i) {
            db->execute(
                "INSERT INTO bench_indexed (category, value, name) VALUES (?, ?, ?)",
                i % 50, static_cast<double>(i), "Name" + std::to_string(i));
        }
    });

    BENCHMARK("Query all 5000 rows") {
        return db->query("SELECT * FROM bench_indexed");
    };

    BENCHMARK("Query with complex WHERE (5000 row table)") {
        return db->query(
            "SELECT * FROM bench_indexed WHERE category IN (1, 5, 10, 15, 20) AND value > 1000");
    };

    BENCHMARK("Query with ORDER BY and LIMIT (5000 row table)") {
        return db->query("SELECT * FROM bench_indexed ORDER BY value DESC LIMIT 100");
    };
}
