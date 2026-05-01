#include "api/route_registry.hpp"
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

crow::response json_response(int status, const nlohmann::json& body) {
    crow::response response{status};
    response.set_header("Content-Type", "application/json");
    response.write(body.dump());
    return response;
}

std::optional<std::string> optional_string(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        return std::nullopt;
    }
    return body.at(key).get<std::string>();
}

std::string required_string(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || !body.at(key).is_string()) {
        throw std::runtime_error{std::string{"missing string field: "} + key};
    }
    return body.at(key).get<std::string>();
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
                required_string(body, "username"),
                optional_string(body, "email"),
                body.value("displayName", std::string{}),
                required_string(body, "password")
            );
            return json_response(201, {
                {"user", user_json(result.user)},
                {"token", result.token},
                {"expiresAt", result.expires_at}
            });
        } catch (const auth::AuthError& error) {
            return json_response(400, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return json_response(400, {{"error", error.what()}});
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
                required_string(body, "identifier"),
                required_string(body, "password"),
                req.get_header_value("User-Agent"),
                req.remote_ip_address.empty()
                    ? std::optional<std::string>{}
                    : std::optional<std::string>{req.remote_ip_address}
            );
            return json_response(200, {
                {"user", user_json(result.user)},
                {"token", result.token},
                {"expiresAt", result.expires_at}
            });
        } catch (const auth::AuthError& error) {
            return json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/me")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            return json_response(200, {{"user", user_json(user)}});
        } catch (const auth::Unauthorized& error) {
            return json_response(401, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/logout")
    .methods(crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req) -> crow::response {
        auth::AuthRepository repository{connection_pool};
        auth::AuthService service{repository};
        const bool revoked = service.logout(auth::bearerToken(req));
        return json_response(200, {{"revoked", revoked}});
    });
});

} // namespace
