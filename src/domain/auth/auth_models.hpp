#pragma once

#include <optional>
#include <string>

namespace auth {

struct User {
    long long id{};
    std::string username;
    std::optional<std::string> email;
    std::string display_name;
    std::string password_hash;
    std::string role{"user"};
    std::string status{"active"};
    std::optional<std::string> last_login_at;
    std::string created_at;
    std::string updated_at;
};

struct PublicUser {
    long long id{};
    std::string username;
    std::optional<std::string> email;
    std::string display_name;
    std::string role;
    std::string status;
};

struct Session {
    long long id{};
    long long user_id{};
    std::string token_hash;
    std::optional<std::string> user_agent;
    std::optional<std::string> ip_address;
    std::string expires_at;
    std::optional<std::string> revoked_at;
};

struct LoginResult {
    PublicUser user;
    std::string token;
    std::string expires_at;
};

inline PublicUser toPublicUser(const User& user) {
    return PublicUser{
        .id = user.id,
        .username = user.username,
        .email = user.email,
        .display_name = user.display_name,
        .role = user.role,
        .status = user.status
    };
}

} // namespace auth
