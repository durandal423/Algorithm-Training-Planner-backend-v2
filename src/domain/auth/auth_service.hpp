#pragma once

#include "domain/auth/auth_models.hpp"
#include "domain/auth/auth_repository.hpp"

#include <optional>
#include <stdexcept>
#include <string>

namespace auth {

class AuthError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class AuthService final {
public:
    explicit AuthService(AuthRepository& repository);

    LoginResult registerUser(
        const std::string& username,
        const std::optional<std::string>& email,
        const std::string& display_name,
        const std::string& password
    ) const;

    LoginResult login(
        const std::string& identifier,
        const std::string& password,
        const std::optional<std::string>& user_agent,
        const std::optional<std::string>& ip_address
    ) const;

    std::optional<PublicUser> currentUser(const std::string& bearer_token) const;
    bool logout(const std::string& bearer_token) const;

    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password_hash, const std::string& password);
    static std::string generateToken();
    static std::string hashToken(const std::string& token);

private:
    static void ensureSodiumReady();
    static void validateNewUser(
        const std::string& username,
        const std::string& display_name,
        const std::string& password
    );

    AuthRepository& repository_;
};

} // namespace auth
