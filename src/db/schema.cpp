#include "db/schema.hpp"

#include "config.hpp"

#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::array kRequiredTables{
    "users",
    "user_sessions",
    "problems",
    "tags",
    "problem_tags",
    "training_goals",
    "training_goal_tags",
    "training_plans",
    "training_plan_items",
    "training_records",
    "background_tasks",
    "judge_configs",
    "judge_test_cases",
    "submissions",
    "submission_results",
    "audit_logs"
};

struct MigrationScript {
    int version;
    std::string_view filename;
    std::string_view name;
};

constexpr std::array kMigrations{
    MigrationScript{1, "migrations/001_core_auth_problem_schema.sql", "create core auth and problem schema"},
    MigrationScript{2, "seeds/002_problem_catalog.sql", "seed problem catalog and tags"},
    MigrationScript{3, "migrations/003_judge_training_admin_schema.sql", "create judge training and admin schema"},
    MigrationScript{4, "migrations/004_background_tasks.sql", "create background task queue schema"},
    MigrationScript{5, "seeds/005_judge_sample_fixtures.sql", "seed sample judge configs and test cases"},
    MigrationScript{6,"seeds/006_default_admin_user.sql","seed default admin user"},
    MigrationScript{9, "migrations/009_training_plan_generation_performance.sql", "optimize training plan generation queries"}
};

std::filesystem::path sql_path(std::string_view filename) {
    return std::filesystem::path{config::BACKEND_SOURCE_DIR}
        / "sql"
        / std::string{filename};
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input{path, std::ios::binary};

    if (!input) {
        throw std::runtime_error{"cannot open SQL file: " + path.string()};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool is_blank(std::string_view text) {
    for (const char ch : text) {
        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t') {
            return false;
        }
    }
    return true;
}

std::string join_names(const std::vector<std::string>& names) {
    std::string result;

    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }

        result += names[i];
    }

    return result;
}

void ensure_public_schema(pqxx::transaction_base& tx) {
    tx.exec(R"SQL(
        CREATE SCHEMA IF NOT EXISTS public
    )SQL").no_rows();
}

void ensure_migration_table(pqxx::transaction_base& tx) {
    tx.exec(R"SQL(
        CREATE TABLE IF NOT EXISTS public.schema_migrations (
            version INTEGER PRIMARY KEY,
            name TEXT NOT NULL,
            applied_at TIMESTAMPTZ NOT NULL DEFAULT now()
        )
    )SQL").no_rows();
}

bool migration_applied(pqxx::transaction_base& tx, int version) {
    return tx.exec(
        R"SQL(
            SELECT EXISTS (
                SELECT 1
                FROM public.schema_migrations
                WHERE version = $1
            )
        )SQL",
        pqxx::params{version}
    ).one_field().as<bool>();
}

void record_migration(
    pqxx::transaction_base& tx,
    const MigrationScript& migration
) {
    tx.exec(
        R"SQL(
            INSERT INTO public.schema_migrations(version, name)
            VALUES ($1, $2)
        )SQL",
        pqxx::params{
            migration.version,
            std::string{migration.name}
        }
    ).no_rows();
}

std::vector<std::string> missing_required_tables(pqxx::transaction_base& tx) {
    std::vector<std::string> missing;

    for (const auto* table : kRequiredTables) {
        const auto qualified = "public." + std::string{table};

        const bool exists = tx.exec(
            R"SQL(
                SELECT to_regclass($1) IS NOT NULL
            )SQL",
            pqxx::params{qualified}
        ).one_field().as<bool>();

        if (!exists) {
            missing.emplace_back(table);
        }
    }

    return missing;
}

void apply_migration(
    pqxx::connection& db,
    const MigrationScript& migration
) {
    const auto path = sql_path(migration.filename);
    const auto sql = read_file(path);

    pqxx::work tx{db};

    tx.exec(R"SQL(
        SET LOCAL search_path TO public
    )SQL").no_rows();

    spdlog::info(
        "applying database migration {} ({}) from {}",
        migration.version,
        migration.name,
        path.string()
    );

    if (!is_blank(sql)) {
        tx.exec(sql).no_rows();
    }

    record_migration(tx, migration);

    tx.commit();

    spdlog::info(
        "database migration {} ({}) applied",
        migration.version,
        migration.name
    );
}

void apply_pending_migrations(pqxx::connection& db) {
    for (const auto& migration : kMigrations) {
        bool already_applied = false;

        {
            pqxx::work tx{db};

            tx.exec(R"SQL(
                SET LOCAL search_path TO public
            )SQL").no_rows();

            already_applied = migration_applied(tx, migration.version);

            tx.commit();
        }

        if (already_applied) {
            spdlog::debug(
                "database migration {} ({}) already applied",
                migration.version,
                migration.name
            );
            continue;
        }

        apply_migration(db, migration);
    }
}

void verify_required_tables(pqxx::connection& db) {
    std::vector<std::string> missing_tables;

    {
        pqxx::work tx{db};

        tx.exec(R"SQL(
            SET LOCAL search_path TO public
        )SQL").no_rows();

        missing_tables = missing_required_tables(tx);

        tx.commit();
    }

    if (!missing_tables.empty()) {
        throw std::runtime_error{
            "database schema is incomplete; missing tables: " +
            join_names(missing_tables)
        };
    }
}

} // namespace

void Schema::ensureSchema(const std::string& migration_conninfo) {
    try {
        pqxx::connection db{migration_conninfo};

        {
            pqxx::work tx{db};

            tx.exec(R"SQL(
                SET LOCAL search_path TO public
            )SQL").no_rows();

            ensure_public_schema(tx);
            ensure_migration_table(tx);

            tx.commit();
        }

        apply_pending_migrations(db);

        verify_required_tables(db);

        spdlog::info("database schema ready");
    } catch (const std::exception& e) {
        spdlog::error("failed to ensure database schema: {}", e.what());
        throw;
    } catch (...) {
        spdlog::error("failed to ensure database schema: unknown exception");
        throw;
    }
}
