#include "db/database.hpp"
#include "db/schema.hpp"

#include "config.hpp"

#include <spdlog/spdlog.h>

std::string conninfo_for_database(const std::string& database_name) {
    return "host=" + std::string(config::APP_DATABASE_HOST) +
           " port=" + std::string(config::APP_DATABASE_PORT) +
           " dbname=" + database_name +
           " user=" + std::string(config::APP_DATABASE_USER) +
           " password=" + std::string(config::APP_DATABASE_PASSWORD) +
           " options='-c search_path=public'";
}

std::string Database::adminConninfo() {
    return conninfo_for_database(config::ADMIN_DATABASE_NAME);
}

std::string Database::appConninfo() {
    return conninfo_for_database(config::APP_DATABASE_NAME);
}

void Database::ensureDatabase() {
    try {
        pqxx::connection admin{adminConninfo()};

        {
            pqxx::work tx{admin};

            const bool exists = tx.exec(
                R"SQL(
                    SELECT EXISTS (
                        SELECT 1
                        FROM pg_database
                        WHERE datname = $1
                    )
                )SQL",
                pqxx::params{config::APP_DATABASE_NAME}
            ).one_field().as<bool>();

            tx.commit();

            if (exists) {
                spdlog::info("database '{}' already exists", config::APP_DATABASE_NAME);
                return;
            }
        }

        {
            pqxx::nontransaction ntx{admin};

            ntx.exec(
                "CREATE DATABASE " +
                ntx.quote_name(config::APP_DATABASE_NAME)
            ).no_rows();

            spdlog::info("database '{}' created", config::APP_DATABASE_NAME);
        }
    } catch (const std::exception& e) {
        spdlog::error("failed to ensure database: {}", e.what());
        throw;
    } catch (...) {
        spdlog::error("failed to ensure database: unknown exception");
        throw;
    }
}

void Database::ensureSchema() {
    Schema::ensureSchema(appConninfo());
}
