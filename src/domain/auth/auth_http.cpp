#include "domain/auth/auth_http.hpp"

#include "domain/auth/auth_repository.hpp"
#include "domain/auth/auth_service.hpp"

#include <string_view>

namespace auth {
namespace {

constexpr std::string_view kBearerPrefix = "Bearer ";

} // namespace

std::string bearerToken(const crow::request& req) {
    const auto header = req.get_header_value("Authorization");
    if (header.rfind(std::string{kBearerPrefix}, 0) != 0) {
        return {};
    }
    return header.substr(kBearerPrefix.size());
}

std::optional<PublicUser> currentUser(ConnectionPool& pool, const crow::request& req) {
    AuthRepository repository{pool};
    AuthService service{repository};
    return service.currentUser(bearerToken(req));
}

PublicUser requireUser(ConnectionPool& pool, const crow::request& req) {
    auto user = currentUser(pool, req);
    if (!user) {
        throw Unauthorized{"authentication required"};
    }
    return *user;
}

PublicUser requireAdmin(ConnectionPool& pool, const crow::request& req) {
    auto user = requireUser(pool, req);
    if (user.role != "admin") {
        throw Forbidden{"admin access required"};
    }
    return user;
}

} // namespace auth
