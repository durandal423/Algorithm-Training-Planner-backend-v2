#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"

#include <exception>
#include <optional>
#include <string>

namespace {

std::optional<long long> task_id_from_ref(const std::string& ref) {
    auto text = ref;
    const auto underscore = text.find('_');
    if (underscore != std::string::npos) {
        text = text.substr(underscore + 1);
    }
    try {
        std::size_t parsed = 0;
        const auto id = std::stoll(text, &parsed);
        if (parsed != text.size() || id <= 0) {
            return std::nullopt;
        }
        return id;
    } catch (...) {
        return std::nullopt;
    }
}

RouteRegistration taskRoutesRegistration([](App& app, runtime::ApplicationContext& context) {
    CROW_ROUTE(app, "/api/tasks/<string>")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req, const std::string& ref) -> crow::response {
        try {
            const auto id = task_id_from_ref(ref);
            if (!id) {
                return api::json_response(404, {{"error", "task not found"}});
            }

            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            const auto owner_filter = user.role == "admin"
                ? std::optional<long long>{}
                : std::optional<long long>{user.id};

            const auto task = context.taskRepository().getTask(*id, owner_filter);
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
