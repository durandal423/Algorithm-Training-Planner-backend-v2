#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/auth/auth_repository.hpp"
#include "domain/auth/auth_service.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <string>

RouteRegistration authRoutesRegistration([](App& app, runtime::ApplicationContext& context) {
    CROW_ROUTE(app, "/api/auth/register")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto body = nlohmann::json::parse(req.body);
            auth::AuthRepository repository{connection_pool};
            auth::AuthService service{repository};
            const auto result = service.registerUser(
                api::required_string(body, "username"),
                body.value("email", std::optional<std::string>{}),
                body.value("displayName", std::string{}),
                api::required_string(body, "password")
            );
            return api::json_response(201, {
                {"user", result.user},
                {"token", result.token},
                {"expiresAt", result.expires_at}
            });
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/login")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto body = nlohmann::json::parse(req.body);
            const auto user_agent = req.get_header_value("User-Agent");
            const auto remote_ip_address = req.remote_ip_address;
            auth::AuthRepository repository{connection_pool};
            auth::AuthService service{repository};
            const auto result = service.login(
                api::required_string(body, "identifier"),
                api::required_string(body, "password"),
                user_agent,
                remote_ip_address.empty()
                    ? std::optional<std::string>{}
                    : std::optional<std::string>{remote_ip_address}
            );
            return api::json_response(200, {
                {"user", result.user},
                {"token", result.token},
                {"expiresAt", result.expires_at}
            });
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/me")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            return api::json_response(200, {{"user", user}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/logout")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        auto& connection_pool = context.apiPool();
        auth::AuthRepository repository{connection_pool};
        auth::AuthService service{repository};
        const bool revoked = service.logout(auth::bearerToken(req));
        return api::json_response(200, {{"revoked", revoked}});
    });
});
