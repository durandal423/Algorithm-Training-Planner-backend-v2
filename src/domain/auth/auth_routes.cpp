#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/auth/auth_repository.hpp"
#include "domain/auth/auth_service.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <string>

namespace {

nlohmann::json user_json(const auth::PublicUser& user) {
    return {
        {"id", user.id},
        {"username", user.username},
        {"email", user.email},
        {"displayName", user.display_name},
        {"role", user.role},
        {"status", user.status}
    };
}

RouteRegistration authRoutesRegistration([](App& app, ConnectionPool& connection_pool) {
    CROW_ROUTE(app, "/api/auth/register")
    .methods(crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto body = nlohmann::json::parse(req.body);
            auth::AuthRepository repository{connection_pool};
            auth::AuthService service{repository};
            const auto result = service.registerUser(
                api::required_string(body, "username"),
                api::optional_string(body, "email"),
                body.value("displayName", std::string{}),
                api::required_string(body, "password")
            );
            return api::json_response(201, {
                {"user", user_json(result.user)},
                {"token", result.token},
                {"expiresAt", result.expires_at}
            });
        } catch (const auth::AuthError& error) {
            return api::json_response(400, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/login")
    .methods(crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto body = nlohmann::json::parse(req.body);
            auth::AuthRepository repository{connection_pool};
            auth::AuthService service{repository};
            const auto result = service.login(
                api::required_string(body, "identifier"),
                api::required_string(body, "password"),
                req.get_header_value("User-Agent"),
                req.remote_ip_address.empty()
                    ? std::optional<std::string>{}
                    : std::optional<std::string>{req.remote_ip_address}
            );
            return api::json_response(200, {
                {"user", user_json(result.user)},
                {"token", result.token},
                {"expiresAt", result.expires_at}
            });
        } catch (const auth::AuthError& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/me")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            return api::json_response(200, {{"user", user_json(user)}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/logout")
    .methods(crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req) -> crow::response {
        auth::AuthRepository repository{connection_pool};
        auth::AuthService service{repository};
        const bool revoked = service.logout(auth::bearerToken(req));
        return api::json_response(200, {{"revoked", revoked}});
    });
});

} // namespace
