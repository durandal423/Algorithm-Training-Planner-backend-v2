#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/judge/judge_repository.hpp"
#include "domain/task/task_models.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <string>

namespace {

void apply_submission_query(judge::SubmissionFilter& filter, const crow::request& req) {
    filter.problem_id = api::optional_query_long(req, "problemId");
    filter.user_id = api::optional_query_long(req, "userId");
    if (const char* status = req.url_params.get("status")) {
        filter.status = status;
    }
    if (const char* verdict = req.url_params.get("verdict")) {
        filter.verdict = verdict;
    }
    if (const auto limit = api::optional_query_int(req, "limit")) {
        filter.limit = *limit;
    }
    if (const auto offset = api::optional_query_int(req, "offset")) {
        filter.offset = *offset;
    }
}

judge::JudgeConfig config_from_json(long long problem_id, const nlohmann::json& body) {
    judge::JudgeConfig config;
    config.problem_id = problem_id;
    config.enabled = body.value("enabled", false);
    config.language = body.value("language", std::string{"cpp"});
    config.compile_command_template = body.value("compileCommandTemplate", std::optional<std::string>{});
    config.run_command_template = body.value("runCommandTemplate", std::optional<std::string>{});
    config.time_limit_ms = body.value("timeLimitMs", 1000);
    config.memory_limit_mb = body.value("memoryLimitMb", 256);
    config.output_limit_kb = body.value("outputLimitKb", 1024);
    config.compare_mode = body.value("compareMode", std::string{"ignore_whitespace"});
    config.float_epsilon = body.value("floatEpsilon", 1e-6);
    return config;
}

judge::JudgeTestCase test_case_from_json(long long problem_id, const nlohmann::json& body) {
    judge::JudgeTestCase test_case;
    test_case.problem_id = problem_id;
    test_case.name = body.value("name", std::string{});
    test_case.input_data = body.at("inputData").get<std::string>();
    test_case.expected_output = body.value("expectedOutput", std::optional<std::string>{});
    test_case.visibility = body.value("visibility", std::string{"hidden"});
    test_case.points = body.value("points", 1);
    test_case.order_index = body.value("orderIndex", 0);
    test_case.time_limit_ms = body.value("timeLimitMs", std::optional<int>{});
    test_case.memory_limit_mb = body.value("memoryLimitMb", std::optional<int>{});
    return test_case;
}

nlohmann::json submission_json(const judge::Submission& submission, bool include_source) {
    auto body = nlohmann::json{
        {"id", submission.id},
        {"problemId", submission.problem_id},
        {"userId", submission.user_id},
        {"language", submission.language},
        {"status", submission.status},
        {"verdict", submission.verdict},
        {"score", submission.score},
        {"compileStdout", submission.compile_stdout},
        {"compileStderr", submission.compile_stderr},
        {"compileTimeMs", submission.compile_time_ms},
        {"maxTimeMs", submission.max_time_ms},
        {"maxMemoryKb", submission.max_memory_kb},
        {"submittedAt", submission.submitted_at},
        {"startedAt", submission.started_at},
        {"finishedAt", submission.finished_at}
    };
    if (include_source) {
        body["sourceCode"] = submission.source_code;
        body["results"] = submission.results;
    }
    return body;
}

RouteRegistration judgeRoutesRegistration([](App& app, runtime::ApplicationContext& context) {
    CROW_ROUTE(app, "/api/submissions")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            judge::SubmissionFilter filter;
            apply_submission_query(filter, req);
            filter.user_id = user.id;

            judge::JudgeRepository repository{connection_pool};
            nlohmann::json items = nlohmann::json::array();
            for (const auto& submission : repository.listSubmissions(filter)) {
                items.push_back(submission_json(submission, false));
            }
            return api::json_response(200, {{"items", items}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/problems/<int>/submissions")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req, int problem_id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            const auto body = nlohmann::json::parse(req.body);
            const auto source_code = body.at("sourceCode").get<std::string>();
            if (source_code.empty() || source_code.size() > 256 * 1024) {
                return api::json_response(400, {{"error", "sourceCode must be 1 to 262144 bytes"}});
            }

            judge::Submission submission;
            submission.problem_id = problem_id;
            submission.user_id = user.id;
            submission.language = body.value("language", std::string{"cpp"});
            submission.source_code = source_code;

            const nlohmann::json payload = {
                {"problemId", problem_id},
                {"language", submission.language},
                {"sourceCode", submission.source_code}
            };
            const auto task_id = context.taskSubmissionService().submit(
                "judge.submission.create",
                user.id,
                payload
            );
            return api::json_response(202, {
                {"submissionTaskId", task::task_ref_for_type("judge.submission.create", task_id)},
                {"status", "pending"}
            });
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/submissions/<int>")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req, int id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            const auto submission = repository.getSubmission(id);
            if (!submission) {
                return api::json_response(404, {{"error", "submission not found"}});
            }
            if (user.role != "admin" && submission->user_id != user.id) {
                return api::json_response(403, {{"error", "submission belongs to another user"}});
            }
            return api::json_response(200, {{"submission", submission_json(*submission, true)}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/admin/submissions")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            auth::requireAdmin(connection_pool, req);
            judge::SubmissionFilter filter;
            apply_submission_query(filter, req);

            judge::JudgeRepository repository{connection_pool};
            nlohmann::json items = nlohmann::json::array();
            for (const auto& submission : repository.listSubmissions(filter)) {
                items.push_back(submission_json(submission, false));
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

    CROW_ROUTE(app, "/api/admin/submissions/<int>/rejudge")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req, int id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            auth::requireAdmin(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            return repository.resetSubmissionForRejudge(id)
                ? api::json_response(200, {{"queued", true}})
                : api::json_response(404, {{"error", "submission not found"}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return api::json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/admin/problems/<string>/judge-config")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Put)
    ([&context](const crow::request& req, const std::string& problem_code) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            auth::requireAdmin(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            const auto problem_id = repository.findProblemIdByCode(problem_code);
            if (!problem_id) {
                return api::json_response(404, {{"error", "problem not found"}});
            }
            if (req.method == crow::HTTPMethod::Get) {
                const auto config = repository.getConfig(*problem_id);
                if (!config) {
                    return api::json_response(404, {{"error", "judge config not found"}});
                }
                return api::json_response(200, {{"config", *config}});
            }

            const auto body = nlohmann::json::parse(req.body);
            const auto config = repository.upsertConfig(config_from_json(*problem_id, body));
            return api::json_response(200, {{"config", config}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return api::json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/admin/problems/<string>/judge-test-cases")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)
    ([&context](const crow::request& req, const std::string& problem_code) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            auth::requireAdmin(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            const auto problem_id = repository.findProblemIdByCode(problem_code);
            if (!problem_id) {
                return api::json_response(404, {{"error", "problem not found"}});
            }
            if (req.method == crow::HTTPMethod::Get) {
                return api::json_response(
                    200,
                    {{"items", repository.listTestCases(*problem_id, true)}}
                );
            }

            const auto body = nlohmann::json::parse(req.body);
            const auto id = repository.createTestCase(test_case_from_json(*problem_id, body));
            return api::json_response(201, {{"id", id}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return api::json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/admin/judge-test-cases/<int>")
    .methods(crow::HTTPMethod::Put, crow::HTTPMethod::Delete)
    ([&context](const crow::request& req, int id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            auth::requireAdmin(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            if (req.method == crow::HTTPMethod::Delete) {
                return repository.deleteTestCase(id)
                    ? api::json_response(200, {{"deleted", true}})
                    : api::json_response(404, {{"error", "judge test case not found"}});
            }

            const auto body = nlohmann::json::parse(req.body);
            const auto updated = repository.updateTestCase(id, test_case_from_json(0, body));
            if (!updated) {
                return api::json_response(404, {{"error", "judge test case not found"}});
            }
            return api::json_response(200, {{"testCase", *updated}});
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
