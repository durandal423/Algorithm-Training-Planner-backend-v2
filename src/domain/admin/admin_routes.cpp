#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/admin/admin_repository.hpp"
#include "domain/auth/auth_http.hpp"

#include <nlohmann/json.hpp>

#include <exception>

namespace {

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
            return api::json_response(200, {{"items", repository.listAuditLogs(limit)}});
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
