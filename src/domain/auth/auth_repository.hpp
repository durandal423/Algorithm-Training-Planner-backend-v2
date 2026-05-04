#pragma once

#include "db/connection_pool.hpp"
#include "domain/auth/auth_models.hpp"

#include <optional>
#include <string>

namespace auth {

template <typename Row>
User user_from_row(const Row& row) {
    User user;
    user.id = row["id"].template as<long long>();
    user.username = row["username"].template as<std::string>();
    user.email = row["email"].template get<std::string>();
    user.display_name = row["display_name"].template as<std::string>();
    user.password_hash = row["password_hash"].template as<std::string>();
    user.role = row["role"].template as<std::string>();
    user.status = row["status"].template as<std::string>();
    user.last_login_at = row["last_login_at"].template get<std::string>();
    user.created_at = row["created_at"].template as<std::string>();
    user.updated_at = row["updated_at"].template as<std::string>();
    return user;
}

class AuthRepository final {
public:
    explicit AuthRepository(ConnectionPool& pool);

    std::optional<User> findUserById(long long id) const;
    std::optional<User> findUserByIdentifier(const std::string& identifier) const;
    bool adminExists() const;
    long long createUser(
        const std::string& username,
        const std::optional<std::string>& email,
        const std::string& display_name,
        const std::string& password_hash,
        const std::string& role
    ) const;

    std::string createSession(
        long long user_id,
        const std::string& token_hash,
        const std::optional<std::string>& user_agent,
        const std::optional<std::string>& ip_address,
        int ttl_hours
    ) const;

    std::optional<User> findUserByActiveSession(const std::string& token_hash) const;
    void touchSession(const std::string& token_hash) const;
    bool revokeSession(const std::string& token_hash) const;
    void markLogin(long long user_id) const;

private:
    ConnectionPool& pool_;
};

} // namespace auth
