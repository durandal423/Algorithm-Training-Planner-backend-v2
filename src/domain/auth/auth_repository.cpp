#include "domain/auth/auth_repository.hpp"

#include <pqxx/pqxx>

namespace auth {
namespace {

std::optional<std::string> optional_string(const pqxx::field& field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<std::string>();
}

std::string string_or_empty(const pqxx::field& field) {
    if (field.is_null()) {
        return {};
    }
    return field.as<std::string>();
}

User user_from_row(const pqxx::row& row) {
    User user;
    user.id = row["id"].as<long long>();
    user.username = row["username"].as<std::string>();
    user.email = optional_string(row["email"]);
    user.display_name = row["display_name"].as<std::string>();
    user.password_hash = row["password_hash"].as<std::string>();
    user.role = row["role"].as<std::string>();
    user.status = row["status"].as<std::string>();
    user.last_login_at = optional_string(row["last_login_at"]);
    user.created_at = string_or_empty(row["created_at"]);
    user.updated_at = string_or_empty(row["updated_at"]);
    return user;
}

constexpr auto kUserSelect = R"SQL(
    SELECT id, username, email, display_name, password_hash, role, status,
           last_login_at, created_at, updated_at
    FROM users
)SQL";

} // namespace

AuthRepository::AuthRepository(ConnectionPool& pool)
    : pool_(pool) {}

std::optional<User> AuthRepository::findUserById(long long id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kUserSelect} + " WHERE id = $1",
        pqxx::params{id}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return user_from_row(*row);
}

std::optional<User> AuthRepository::findUserByIdentifier(const std::string& identifier) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kUserSelect} + R"SQL(
            WHERE lower(username) = lower($1)
               OR (email IS NOT NULL AND lower(email) = lower($1))
        )SQL",
        pqxx::params{identifier}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return user_from_row(*row);
}

long long AuthRepository::createUser(
    const std::string& username,
    const std::optional<std::string>& email,
    const std::string& display_name,
    const std::string& password_hash,
    const std::string& role
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO users(username, email, display_name, password_hash, role)
            VALUES ($1, $2, $3, $4, $5)
            RETURNING id
        )SQL",
        pqxx::params{username, email, display_name, password_hash, role}
    ).one_field().as<long long>();
    tx.commit();
    return id;
}

std::string AuthRepository::createSession(
    long long user_id,
    const std::string& token_hash,
    const std::optional<std::string>& user_agent,
    const std::optional<std::string>& ip_address,
    int ttl_hours
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto expires_at = tx.exec(
        R"SQL(
            INSERT INTO user_sessions(user_id, token_hash, user_agent, ip_address, expires_at)
            VALUES ($1, $2, $3, $4::inet, now() + ($5::text || ' hours')::interval)
            RETURNING expires_at
        )SQL",
        pqxx::params{user_id, token_hash, user_agent, ip_address, ttl_hours}
    ).one_field().as<std::string>();
    tx.commit();
    return expires_at;
}

std::optional<User> AuthRepository::findUserByActiveSession(const std::string& token_hash) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kUserSelect} + R"SQL(
            JOIN user_sessions s ON s.user_id = users.id
            WHERE s.token_hash = $1
              AND s.revoked_at IS NULL
              AND s.expires_at > now()
              AND users.status = 'active'
        )SQL",
        pqxx::params{token_hash}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return user_from_row(*row);
}

void AuthRepository::touchSession(const std::string& token_hash) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        R"SQL(
            UPDATE user_sessions
            SET last_seen_at = now()
            WHERE token_hash = $1
              AND revoked_at IS NULL
              AND expires_at > now()
        )SQL",
        pqxx::params{token_hash}
    ).no_rows();
    tx.commit();
}

bool AuthRepository::revokeSession(const std::string& token_hash) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        R"SQL(
            UPDATE user_sessions
            SET revoked_at = now()
            WHERE token_hash = $1
              AND revoked_at IS NULL
        )SQL",
        pqxx::params{token_hash}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

void AuthRepository::markLogin(long long user_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        R"SQL(
            UPDATE users
            SET last_login_at = now()
            WHERE id = $1
        )SQL",
        pqxx::params{user_id}
    ).no_rows();
    tx.commit();
}

} // namespace auth
