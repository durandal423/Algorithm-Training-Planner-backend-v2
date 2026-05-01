#include "domain/auth/bootstrap_admin.hpp"

#include "domain/auth/auth_repository.hpp"
#include "domain/auth/auth_service.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>

namespace auth {
namespace {

std::string env_value(const char* key) {
    const char* value = std::getenv(key);
    return value == nullptr ? std::string{} : std::string{value};
}

std::string trim_copy(std::string text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch);
    });
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char ch) {
        return std::isspace(ch);
    }).base();
    if (first >= last) {
        return {};
    }
    return std::string{first, last};
}

} // namespace

void bootstrapAdminFromEnv(ConnectionPool& pool) {
    AuthRepository repository{pool};
    if (repository.adminExists()) {
        spdlog::debug("admin bootstrap skipped because an admin user already exists");
        return;
    }

    const auto username = trim_copy(env_value("APP_BOOTSTRAP_ADMIN_USERNAME"));
    const auto email = trim_copy(env_value("APP_BOOTSTRAP_ADMIN_EMAIL"));
    const auto password = env_value("APP_BOOTSTRAP_ADMIN_PASSWORD");

    if (username.empty() && email.empty() && password.empty()) {
        spdlog::info("admin bootstrap skipped because APP_BOOTSTRAP_ADMIN_* is not configured");
        return;
    }
    if (username.empty() || email.empty() || password.empty()) {
        spdlog::warn("admin bootstrap skipped because APP_BOOTSTRAP_ADMIN_* is incomplete");
        return;
    }

    const auto password_hash = AuthService::hashPassword(password);
    const auto user_id = repository.createUser(
        username,
        std::optional<std::string>{email},
        username,
        password_hash,
        "admin"
    );

    spdlog::info("bootstrap admin user created (id={}, username={})", user_id, username);
}

} // namespace auth
