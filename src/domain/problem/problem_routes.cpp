#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/problem/problem_repository.hpp"

#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace {

std::vector<std::string> tag_names_from_json(const nlohmann::json& body) {
    return api::string_array(body, "tags");
}

problem::Problem problem_from_json(
    const nlohmann::json& body,
    std::optional<long long> created_by = std::nullopt
) {
    problem::Problem problem;
    problem.code = body.at("code").get<std::string>();
    problem.title = body.at("title").get<std::string>();
    problem.source_platform = body.value("sourcePlatform", std::string{"local"});
    problem.source_url = api::optional_string(body, "sourceUrl");
    problem.difficulty = body.at("difficulty").get<int>();
    problem.estimated_minutes = body.value("estimatedMinutes", 30);
    problem.description = body.value("description", std::string{});
    problem.input_format = body.value("inputFormat", std::string{});
    problem.output_format = body.value("outputFormat", std::string{});
    problem.sample_input = body.value("sampleInput", std::string{});
    problem.sample_output = body.value("sampleOutput", std::string{});
    problem.hint = body.value("hint", std::string{});
    problem.visibility = body.value("visibility", std::string{"public"});
    problem.created_by = created_by;
    return problem;
}

bool is_admin(ConnectionPool& pool, const crow::request& req) {
    const auto user = auth::currentUser(pool, req);
    return user && user->role == "admin";
}

RouteRegistration problemRoutesRegistration([](App& app, ConnectionPool& connection_pool) {
    CROW_ROUTE(app, "/api/problems")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req) -> crow::response {
        problem::ProblemFilter filter;
        filter.include_hidden = is_admin(connection_pool, req);
        if (const char* q = req.url_params.get("q")) {
            filter.keyword = q;
        }
        if (const char* tag = req.url_params.get("tag")) {
            filter.tag = tag;
        }
        if (const auto limit = api::optional_query_int(req, "limit")) {
            filter.limit = *limit;
        }
        if (const auto offset = api::optional_query_int(req, "offset")) {
            filter.offset = *offset;
        }

        problem::ProblemRepository repository{connection_pool};
        return api::json_response(200, {{"items", repository.listProblems(filter)}});
    });

    CROW_ROUTE(app, "/api/problems/<int>")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req, int id) -> crow::response {
        problem::ProblemRepository repository{connection_pool};
        const auto item = repository.getProblem(id, is_admin(connection_pool, req));
        if (!item) {
            return api::json_response(404, {{"error", "problem not found"}});
        }
        return api::json_response(200, {{"problem", *item}});
    });

    CROW_ROUTE(app, "/api/tags")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool]() -> crow::response {
        problem::ProblemRepository repository{connection_pool};
        return api::json_response(200, {{"items", repository.listTags()}});
    });

    CROW_ROUTE(app, "/api/admin/problems")
    .methods(crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto admin = auth::requireAdmin(connection_pool, req);
            const auto body = nlohmann::json::parse(req.body);
            problem::ProblemRepository repository{connection_pool};
            const auto id = repository.createProblem(
                problem_from_json(body, admin.id),
                tag_names_from_json(body)
            );
            return api::json_response(201, {{"id", id}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return api::json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/admin/problems/<int>")
    .methods(crow::HTTPMethod::Put, crow::HTTPMethod::Delete)
    ([&connection_pool](const crow::request& req, int id) -> crow::response {
        try {
            auth::requireAdmin(connection_pool, req);
            problem::ProblemRepository repository{connection_pool};
            if (req.method == crow::HTTPMethod::Delete) {
                return repository.deleteProblem(id)
                    ? api::json_response(200, {{"deleted", true}})
                    : api::json_response(404, {{"error", "problem not found"}});
            }
            const auto body = nlohmann::json::parse(req.body);
            const bool updated = repository.updateProblem(
                id,
                problem_from_json(body),
                tag_names_from_json(body)
            );
            return updated
                ? api::json_response(200, {{"updated", true}})
                : api::json_response(404, {{"error", "problem not found"}});
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
