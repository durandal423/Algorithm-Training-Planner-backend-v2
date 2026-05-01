#include "api/route_registry.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/problem/problem_repository.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>
#include <vector>

namespace {

nlohmann::json tag_json(const problem::Tag& tag) {
    return {
        {"id", tag.id},
        {"name", tag.name},
        {"description", tag.description}
    };
}

nlohmann::json problem_json(const problem::Problem& problem) {
    nlohmann::json tags = nlohmann::json::array();
    for (const auto& tag : problem.tags) {
        tags.push_back(tag_json(tag));
    }
    return {
        {"id", problem.id},
        {"code", problem.code},
        {"title", problem.title},
        {"sourcePlatform", problem.source_platform},
        {"sourceUrl", problem.source_url},
        {"difficulty", problem.difficulty},
        {"estimatedMinutes", problem.estimated_minutes},
        {"description", problem.description},
        {"inputFormat", problem.input_format},
        {"outputFormat", problem.output_format},
        {"sampleInput", problem.sample_input},
        {"sampleOutput", problem.sample_output},
        {"hint", problem.hint},
        {"visibility", problem.visibility},
        {"createdBy", problem.created_by},
        {"createdAt", problem.created_at},
        {"updatedAt", problem.updated_at},
        {"tags", tags}
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

std::vector<std::string> tag_names_from_json(const nlohmann::json& body) {
    std::vector<std::string> tags;
    if (!body.contains("tags") || !body.at("tags").is_array()) {
        return tags;
    }
    for (const auto& tag : body.at("tags")) {
        if (tag.is_string()) {
            tags.push_back(tag.get<std::string>());
        }
    }
    return tags;
}

problem::Problem problem_from_json(
    const nlohmann::json& body,
    std::optional<long long> created_by = std::nullopt
) {
    problem::Problem problem;
    problem.code = body.at("code").get<std::string>();
    problem.title = body.at("title").get<std::string>();
    problem.source_platform = body.value("sourcePlatform", std::string{"local"});
    problem.source_url = optional_string(body, "sourceUrl");
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
        if (const char* limit = req.url_params.get("limit")) {
            filter.limit = std::stoi(limit);
        }
        if (const char* offset = req.url_params.get("offset")) {
            filter.offset = std::stoi(offset);
        }

        problem::ProblemRepository repository{connection_pool};
        nlohmann::json items = nlohmann::json::array();
        for (const auto& problem : repository.listProblems(filter)) {
            items.push_back(problem_json(problem));
        }
        return json_response(200, {{"items", items}});
    });

    CROW_ROUTE(app, "/api/problems/<int>")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req, int id) -> crow::response {
        problem::ProblemRepository repository{connection_pool};
        const auto item = repository.getProblem(id, is_admin(connection_pool, req));
        if (!item) {
            return json_response(404, {{"error", "problem not found"}});
        }
        return json_response(200, {{"problem", problem_json(*item)}});
    });

    CROW_ROUTE(app, "/api/tags")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool]() -> crow::response {
        problem::ProblemRepository repository{connection_pool};
        nlohmann::json items = nlohmann::json::array();
        for (const auto& tag : repository.listTags()) {
            items.push_back(tag_json(tag));
        }
        return json_response(200, {{"items", items}});
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
            return json_response(201, {{"id", id}});
        } catch (const auth::Unauthorized& error) {
            return json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return json_response(400, {{"error", error.what()}});
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
                    ? json_response(200, {{"deleted", true}})
                    : json_response(404, {{"error", "problem not found"}});
            }
            const auto body = nlohmann::json::parse(req.body);
            const bool updated = repository.updateProblem(
                id,
                problem_from_json(body),
                tag_names_from_json(body)
            );
            return updated
                ? json_response(200, {{"updated", true}})
                : json_response(404, {{"error", "problem not found"}});
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
