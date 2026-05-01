#pragma once

#include "db/connection_pool.hpp"
#include "domain/auth/auth_models.hpp"

#include <optional>
#include <string>

namespace auth {

class AuthRepository final {
public:
    explicit AuthRepository(ConnectionPool& pool);

    std::optional<User> findUserById(long long id) const;
    std::optional<User> findUserByIdentifier(const std::string& identifier) const;
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
