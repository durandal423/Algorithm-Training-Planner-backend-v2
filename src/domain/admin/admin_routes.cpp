#include "api/route_registry.hpp"
#include "domain/admin/admin_repository.hpp"
#include "domain/auth/auth_http.hpp"

#include <nlohmann/json.hpp>

#include <exception>

namespace {

crow::response json_response(int status, const nlohmann::json& body) {
    crow::response response{status};
    response.set_header("Content-Type", "application/json");
    response.write(body.dump());
    return response;
}

nlohmann::json audit_log_json(const admin::AuditLog& log) {
    return {
        {"id", log.id},
        {"actorUserId", log.actor_user_id},
        {"action", log.action},
        {"targetType", log.target_type},
        {"targetId", log.target_id},
        {"metadata", nlohmann::json::parse(log.metadata_json)},
        {"createdAt", log.created_at}
    };
}

RouteRegistration adminRoutesRegistration([](App& app, ConnectionPool& connection_pool) {
    CROW_ROUTE(app, "/api/admin/audit-logs")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            auth::requireAdmin(connection_pool, req);
            int limit = 100;
            if (const char* value = req.url_params.get("limit")) {
                limit = std::stoi(value);
            }
            admin::AdminRepository repository{connection_pool};
            nlohmann::json items = nlohmann::json::array();
            for (const auto& log : repository.listAuditLogs(limit)) {
                items.push_back(audit_log_json(log));
            }
            return json_response(200, {{"items", items}});
        } catch (const auth::Unauthorized& error) {
            return json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return json_response(400, {{"error", error.what()}});
        }
    });
});

} // namespace
