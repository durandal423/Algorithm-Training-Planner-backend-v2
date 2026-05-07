#pragma once

#include "db/connection_pool.hpp"
#include "domain/judge/judge_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace judge {

template <typename Row>
JudgeConfig config_from_row(const Row& row) {
    JudgeConfig config;
    config.id = row["id"].template as<long long>();
    config.problem_id = row["problem_id"].template as<long long>();
    config.enabled = row["enabled"].template as<bool>();
    config.language = row["language"].template as<std::string>();
    config.compile_command_template = row["compile_command_template"].template get<std::string>();
    config.run_command_template = row["run_command_template"].template get<std::string>();
    config.time_limit_ms = row["time_limit_ms"].template as<int>();
    config.memory_limit_mb = row["memory_limit_mb"].template as<int>();
    config.output_limit_kb = row["output_limit_kb"].template as<int>();
    config.compare_mode = row["compare_mode"].template as<std::string>();
    config.float_epsilon = row["float_epsilon"].template as<double>();
    return config;
}

template <typename Row>
JudgeTestCase test_case_from_row(const Row& row) {
    JudgeTestCase test_case;
    test_case.id = row["id"].template as<long long>();
    test_case.problem_id = row["problem_id"].template as<long long>();
    test_case.name = row["name"].template as<std::string>("");
    test_case.input_data = row["input_data"].template as<std::string>();
    test_case.expected_output = row["expected_output"].template get<std::string>();
    test_case.visibility = row["visibility"].template as<std::string>();
    test_case.points = row["points"].template as<int>();
    test_case.order_index = row["order_index"].template as<int>();
    test_case.time_limit_ms = row["time_limit_ms"].template get<int>();
    test_case.memory_limit_mb = row["memory_limit_mb"].template get<int>();
    return test_case;
}

template <typename Row>
Submission submission_from_row(const Row& row) {
    Submission submission;
    submission.id = row["id"].template as<long long>();
    submission.problem_id = row["problem_id"].template as<long long>();
    submission.user_id = row["user_id"].template get<long long>();
    submission.language = row["language"].template as<std::string>();
    submission.source_code = row["source_code"].template as<std::string>();
    submission.status = row["status"].template as<std::string>();
    submission.verdict = row["verdict"].template get<std::string>();
    submission.score = row["score"].template as<int>();
    submission.compile_stdout = row["compile_stdout"].template as<std::string>("");
    submission.compile_stderr = row["compile_stderr"].template as<std::string>("");
    submission.compile_time_ms = row["compile_time_ms"].template get<int>();
    submission.max_time_ms = row["max_time_ms"].template get<int>();
    submission.max_memory_kb = row["max_memory_kb"].template get<int>();
    submission.submitted_at = row["submitted_at"].template as<std::string>("");
    submission.started_at = row["started_at"].template get<std::string>();
    submission.finished_at = row["finished_at"].template get<std::string>();
    return submission;
}

template <typename Row>
Submission submission_summary_from_row(const Row& row) {
    Submission submission;
    submission.id = row["id"].template as<long long>();
    submission.problem_id = row["problem_id"].template as<long long>();
    submission.user_id = row["user_id"].template get<long long>();
    submission.language = row["language"].template as<std::string>();
    submission.status = row["status"].template as<std::string>();
    submission.verdict = row["verdict"].template get<std::string>();
    submission.score = row["score"].template as<int>();
    submission.compile_stdout = row["compile_stdout"].template as<std::string>("");
    submission.compile_stderr = row["compile_stderr"].template as<std::string>("");
    submission.compile_time_ms = row["compile_time_ms"].template get<int>();
    submission.max_time_ms = row["max_time_ms"].template get<int>();
    submission.max_memory_kb = row["max_memory_kb"].template get<int>();
    submission.submitted_at = row["submitted_at"].template as<std::string>("");
    submission.started_at = row["started_at"].template get<std::string>();
    submission.finished_at = row["finished_at"].template get<std::string>();
    return submission;
}

template <typename Row>
SubmissionResult result_from_row(const Row& row) {
    SubmissionResult result;
    result.id = row["id"].template as<long long>();
    result.submission_id = row["submission_id"].template as<long long>();
    result.test_case_id = row["test_case_id"].template get<long long>();
    result.order_index = row["order_index"].template as<int>();
    result.status = row["status"].template as<std::string>();
    result.verdict = row["verdict"].template get<std::string>();
    result.time_ms = row["time_ms"].template get<int>();
    result.memory_kb = row["memory_kb"].template get<int>();
    result.exit_code = row["exit_code"].template get<int>();
    result.stdout_sample = row["stdout_sample"].template as<std::string>("");
    result.stderr_sample = row["stderr_sample"].template as<std::string>("");
    result.message = row["message"].template as<std::string>("");
    return result;
}

class JudgeRepository final {
public:
    explicit JudgeRepository(ConnectionPool& pool);

    std::optional<long long> findProblemIdByCode(const std::string& problem_code) const;
    std::optional<JudgeConfig> getConfig(long long problem_id) const;
    JudgeConfig upsertConfig(const JudgeConfig& config) const;
    std::vector<JudgeTestCase> listTestCases(long long problem_id, bool include_hidden) const;
    std::vector<JudgeTestCase> listJudgingTestCases(long long problem_id) const;
    std::optional<JudgeTestCase> getTestCase(long long id) const;
    long long createTestCase(const JudgeTestCase& test_case) const;
    std::optional<JudgeTestCase> updateTestCase(long long id, const JudgeTestCase& test_case) const;
    bool deleteTestCase(long long id) const;

    long long createSubmission(const Submission& submission) const;
    std::optional<Submission> getSubmission(long long id) const;
    std::vector<Submission> listSubmissions(const SubmissionFilter& filter) const;
    std::optional<Submission> fetchNextQueuedSubmission(const std::string& worker_id) const;
    bool refreshSubmissionLease(long long id, const std::string& worker_id) const;
    int recoverStaleJudgingSubmissions(int stale_after_seconds, int max_attempts) const;
    bool resetSubmissionForRejudge(long long id) const;

    bool insertSubmissionResult(const SubmissionResult& result, const std::string& worker_id) const;
    bool completeSubmission(
        long long id,
        const std::string& worker_id,
        const std::string& verdict,
        int score,
        const std::string& compile_stdout,
        const std::string& compile_stderr,
        std::optional<int> compile_time_ms,
        std::optional<int> max_time_ms,
        std::optional<int> max_memory_kb
    ) const;
    bool failSubmission(
        long long id,
        const std::string& worker_id,
        const std::string& verdict,
        const std::string& message,
        const std::string& compile_stdout = {},
        const std::string& compile_stderr = {}
    ) const;

private:
    ConnectionPool& pool_;

    std::vector<SubmissionResult> loadResults(pqxx::transaction_base& tx, long long submission_id) const;
    void updateLatestPlanItemFromSubmission(
        pqxx::transaction_base& tx,
        long long submission_id,
        const std::string& verdict
    ) const;
};

} // namespace judge
