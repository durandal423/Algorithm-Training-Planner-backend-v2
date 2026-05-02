#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

namespace judge {

struct JudgeConfig {
    long long id{};
    long long problem_id{};
    bool enabled{};
    std::string language{"cpp"};
    std::optional<std::string> compile_command_template;
    std::optional<std::string> run_command_template;
    int time_limit_ms{1000};
    int memory_limit_mb{256};
    int output_limit_kb{1024};
    std::string compare_mode{"ignore_whitespace"};
    double float_epsilon{1e-6};
};

inline void to_json(nlohmann::json& json, const JudgeConfig& config) {
    json = {
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

struct JudgeTestCase {
    long long id{};
    long long problem_id{};
    std::string name;
    std::string input_data;
    std::optional<std::string> expected_output;
    std::string visibility{"hidden"};
    int points{1};
    int order_index{};
    std::optional<int> time_limit_ms;
    std::optional<int> memory_limit_mb;
};

inline void to_json(nlohmann::json& json, const JudgeTestCase& test_case) {
    json = {
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

struct SubmissionResult {
    long long id{};
    long long submission_id{};
    std::optional<long long> test_case_id;
    int order_index{};
    std::string status{"completed"};
    std::optional<std::string> verdict;
    std::optional<int> time_ms;
    std::optional<int> memory_kb;
    std::optional<int> exit_code;
    std::string stdout_sample;
    std::string stderr_sample;
    std::string message;
};

inline void to_json(nlohmann::json& json, const SubmissionResult& result) {
    json = {
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

struct Submission {
    long long id{};
    long long problem_id{};
    std::optional<long long> user_id;
    std::string language{"cpp"};
    std::string source_code;
    std::string status{"queued"};
    std::optional<std::string> verdict;
    int score{};
    std::string compile_stdout;
    std::string compile_stderr;
    std::optional<int> compile_time_ms;
    std::optional<int> max_time_ms;
    std::optional<int> max_memory_kb;
    std::string submitted_at;
    std::optional<std::string> started_at;
    std::optional<std::string> finished_at;
    std::vector<SubmissionResult> results;
};

struct SubmissionFilter {
    std::optional<long long> problem_id;
    std::optional<long long> user_id;
    std::string status;
    std::string verdict;
    int limit{50};
    int offset{};
};

} // namespace judge
