#include "api/route_registry.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/judge/judge_repository.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <string>

namespace {

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

std::optional<int> optional_int(const nlohmann::json& body, const char* key) {
    if (!body.contains(key) || body.at(key).is_null()) {
        return std::nullopt;
    }
    return body.at(key).get<int>();
}

nlohmann::json config_json(const judge::JudgeConfig& config) {
    return {
        {"id", config.id},
        {"problemId", config.problem_id},
        {"enabled", config.enabled},
        {"language", config.language},
        {"compileCommandTemplate", config.compile_command_template},
        {"runCommandTemplate", config.run_command_template},
        {"timeLimitMs", config.time_limit_ms},
        {"memoryLimitMb", config.memory_limit_mb},
        {"outputLimitKb", config.output_limit_kb},
        {"compareMode", config.compare_mode},
        {"floatEpsilon", config.float_epsilon}
    };
}

judge::JudgeConfig config_from_json(long long problem_id, const nlohmann::json& body) {
    judge::JudgeConfig config;
    config.problem_id = problem_id;
    config.enabled = body.value("enabled", false);
    config.language = body.value("language", std::string{"cpp"});
    config.compile_command_template = optional_string(body, "compileCommandTemplate");
    config.run_command_template = optional_string(body, "runCommandTemplate");
    config.time_limit_ms = body.value("timeLimitMs", 1000);
    config.memory_limit_mb = body.value("memoryLimitMb", 256);
    config.output_limit_kb = body.value("outputLimitKb", 1024);
    config.compare_mode = body.value("compareMode", std::string{"ignore_whitespace"});
    config.float_epsilon = body.value("floatEpsilon", 1e-6);
    return config;
}

nlohmann::json test_case_json(const judge::JudgeTestCase& test_case) {
    return {
        {"id", test_case.id},
        {"problemId", test_case.problem_id},
        {"name", test_case.name},
        {"inputData", test_case.input_data},
        {"expectedOutput", test_case.expected_output},
        {"visibility", test_case.visibility},
        {"points", test_case.points},
        {"orderIndex", test_case.order_index},
        {"timeLimitMs", test_case.time_limit_ms},
        {"memoryLimitMb", test_case.memory_limit_mb}
    };
}

judge::JudgeTestCase test_case_from_json(long long problem_id, const nlohmann::json& body) {
    judge::JudgeTestCase test_case;
    test_case.problem_id = problem_id;
    test_case.name = body.value("name", std::string{});
    test_case.input_data = body.at("inputData").get<std::string>();
    test_case.expected_output = optional_string(body, "expectedOutput");
    test_case.visibility = body.value("visibility", std::string{"hidden"});
    test_case.points = body.value("points", 1);
    test_case.order_index = body.value("orderIndex", 0);
    test_case.time_limit_ms = optional_int(body, "timeLimitMs");
    test_case.memory_limit_mb = optional_int(body, "memoryLimitMb");
    return test_case;
}

nlohmann::json result_json(const judge::SubmissionResult& result) {
    return {
        {"id", result.id},
        {"submissionId", result.submission_id},
        {"testCaseId", result.test_case_id},
        {"orderIndex", result.order_index},
        {"status", result.status},
        {"verdict", result.verdict},
        {"timeMs", result.time_ms},
        {"memoryKb", result.memory_kb},
        {"exitCode", result.exit_code},
        {"stdoutSample", result.stdout_sample},
        {"stderrSample", result.stderr_sample},
        {"message", result.message}
    };
}

nlohmann::json submission_json(const judge::Submission& submission) {
    nlohmann::json results = nlohmann::json::array();
    for (const auto& result : submission.results) {
        results.push_back(result_json(result));
    }
    return {
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
        {"finishedAt", submission.finished_at},
        {"results", results}
    };
}

RouteRegistration judgeRoutesRegistration([](App& app, ConnectionPool& connection_pool) {
    CROW_ROUTE(app, "/api/problems/<int>/submissions")
    .methods(crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req, int problem_id) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            const auto body = nlohmann::json::parse(req.body);
            const auto source_code = body.at("sourceCode").get<std::string>();
            if (source_code.empty() || source_code.size() > 256 * 1024) {
                return json_response(400, {{"error", "sourceCode must be 1 to 262144 bytes"}});
            }

            judge::Submission submission;
            submission.problem_id = problem_id;
            submission.user_id = user.id;
            submission.language = body.value("language", std::string{"cpp"});
            submission.source_code = source_code;

            judge::JudgeRepository repository{connection_pool};
            const auto id = repository.createSubmission(submission);
            return json_response(201, {{"id", id}, {"status", "queued"}});
        } catch (const auth::Unauthorized& error) {
            return json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/submissions/<int>")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req, int id) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            const auto submission = repository.getSubmission(id);
            if (!submission) {
                return json_response(404, {{"error", "submission not found"}});
            }
            if (user.role != "admin" && submission->user_id != user.id) {
                return json_response(403, {{"error", "submission belongs to another user"}});
            }
            return json_response(200, {{"submission", submission_json(*submission)}});
        } catch (const auth::Unauthorized& error) {
            return json_response(401, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/admin/problems/<int>/judge-config")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Put)
    ([&connection_pool](const crow::request& req, int problem_id) -> crow::response {
        try {
            auth::requireAdmin(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            if (req.method == crow::HTTPMethod::Get) {
                const auto config = repository.getConfig(problem_id);
                if (!config) {
                    return json_response(404, {{"error", "judge config not found"}});
                }
                return json_response(200, {{"config", config_json(*config)}});
            }

            const auto body = nlohmann::json::parse(req.body);
            const auto config = repository.upsertConfig(config_from_json(problem_id, body));
            return json_response(200, {{"config", config_json(config)}});
        } catch (const auth::Unauthorized& error) {
            return json_response(401, {{"error", error.what()}});
        } catch (const auth::Forbidden& error) {
            return json_response(403, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/admin/problems/<int>/judge-test-cases")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req, int problem_id) -> crow::response {
        try {
            auth::requireAdmin(connection_pool, req);
            judge::JudgeRepository repository{connection_pool};
            if (req.method == crow::HTTPMethod::Get) {
                nlohmann::json items = nlohmann::json::array();
                for (const auto& test_case : repository.listTestCases(problem_id, true)) {
                    items.push_back(test_case_json(test_case));
                }
                return json_response(200, {{"items", items}});
            }

            const auto body = nlohmann::json::parse(req.body);
            const auto id = repository.createTestCase(test_case_from_json(problem_id, body));
            return json_response(201, {{"id", id}});
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
