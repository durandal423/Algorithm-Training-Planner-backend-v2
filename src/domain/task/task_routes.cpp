#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"

#include <exception>
#include <optional>

namespace {

RouteRegistration taskRoutesRegistration([](App& app, runtime::ApplicationContext& context) {
    CROW_ROUTE(app, "/api/tasks/<int>")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req, int id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            const auto owner_filter = user.role == "admin"
                ? std::optional<long long>{}
                : std::optional<long long>{user.id};

            const auto task = context.taskRepository().getTask(id, owner_filter);
            if (!task) {
                return api::json_response(404, {{"error", "task not found"}});
            }
            return api::json_response(200, {{"task", *task}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });
});

} // namespace
