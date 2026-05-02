#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/admin/admin_repository.hpp"
#include "domain/auth/auth_http.hpp"

#include <nlohmann/json.hpp>

#include <exception>

namespace {

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
            if (const auto value = api::optional_query_int(req, "limit")) {
                limit = *value;
            }
            admin::AdminRepository repository{connection_pool};
            nlohmann::json items = nlohmann::json::array();
            for (const auto& log : repository.listAuditLogs(limit)) {
                items.push_back(audit_log_json(log));
            }
            return api::json_response(200, {{"items", items}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return api::json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });
});

} // namespace
