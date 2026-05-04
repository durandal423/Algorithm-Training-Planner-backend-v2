#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/auth/auth_repository.hpp"
#include "domain/auth/auth_service.hpp"
#include "domain/task/task_models.hpp"
#include "domain/task/task_scheduler.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <string>

namespace {

std::string poll_url(const std::string& task_ref, const std::string& task_token) {
    return "/api/tasks/" + task_ref + "?taskToken=" + task_token;
}

nlohmann::json auth_task_response(long long task_id, const std::string& type, const std::string& task_token) {
    const auto task_ref = task::task_ref_for_type(type, task_id);
    return {
        {"authTaskId", task_ref},
        {"status", "pending"},
        {"taskToken", task_token},
        {"pollUrl", poll_url(task_ref, task_token)}
    };
}

RouteRegistration authRoutesRegistration([](App& app, runtime::ApplicationContext& context) {
    CROW_ROUTE(app, "/api/auth/register")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        constexpr auto kTaskType = "auth.register";
        const auto body_text = req.body;
        const auto task_token = auth::AuthService::generateToken();
        const nlohmann::json payload = {
            {"taskToken", task_token}
        };
        try {
            const auto task_id = context.taskScheduler().submit(
                kTaskType,
                std::nullopt,
                payload,
                [&context, body_text](long long task_id) {
                    auto& connection_pool = context.apiPool();
                    const auto body = nlohmann::json::parse(body_text);
                    auth::AuthRepository repository{connection_pool};
                    auth::AuthService service{repository};
                    const auto result = service.registerUser(
                        api::required_string(body, "username"),
                        body.value("email", std::optional<std::string>{}),
                        body.value("displayName", std::string{}),
                        api::required_string(body, "password")
                    );
                    return nlohmann::json{
                        {"authTaskId", task::task_ref_for_type("auth.register", task_id)},
                        {"user", result.user},
                        {"token", result.token},
                        {"expiresAt", result.expires_at}
                    };
                }
            );
            return api::json_response(202, auth_task_response(task_id, kTaskType, task_token));
        } catch (const task::TaskQueueFull& error) {
            return api::json_response(503, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/auth/login")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        constexpr auto kTaskType = "auth.login";
        const auto body_text = req.body;
        const auto user_agent = req.get_header_value("User-Agent");
        const auto remote_ip_address = req.remote_ip_address;
        const auto task_token = auth::AuthService::generateToken();
        const nlohmann::json payload = {
            {"taskToken", task_token}
        };
        try {
            const auto task_id = context.taskScheduler().submit(
                kTaskType,
                std::nullopt,
                payload,
                [&context, body_text, user_agent, remote_ip_address](long long task_id) {
                    auto& connection_pool = context.apiPool();
                    const auto body = nlohmann::json::parse(body_text);
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
                    return nlohmann::json{
                        {"authTaskId", task::task_ref_for_type("auth.login", task_id)},
                        {"user", result.user},
                        {"token", result.token},
                        {"expiresAt", result.expires_at}
                    };
                }
            );
            return api::json_response(202, auth_task_response(task_id, kTaskType, task_token));
        } catch (const task::TaskQueueFull& error) {
            return api::json_response(503, {{"error", error.what()}});
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

} // namespace
