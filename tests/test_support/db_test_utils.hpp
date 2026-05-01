#pragma once

#include "db/database.hpp"

#include <pqxx/pqxx>

#include <chrono>
#include <cstdlib>
#include <string>

namespace test_support {

inline bool dbTestsEnabled() {
    return std::getenv("RUN_DB_TESTS") != nullptr;
}

inline void ensureDatabaseReady() {
    Database::ensureDatabase();
    Database::ensureSchema();
}

inline std::string uniqueName(const std::string& prefix) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return prefix + std::to_string(stamp);
}

inline void deleteUserByUsername(const std::string& username) {
    pqxx::connection conn{Database::appConninfo()};
    pqxx::work tx{conn};
    tx.exec(
        "DELETE FROM users WHERE username = $1",
        pqxx::params{username}
    ).no_rows();
    tx.commit();
}

inline void deleteProblemByCode(const std::string& code) {
    pqxx::connection conn{Database::appConninfo()};
    pqxx::work tx{conn};
    tx.exec(
        "DELETE FROM problems WHERE code = $1",
        pqxx::params{code}
    ).no_rows();
    tx.commit();
}

} // namespace test_support
