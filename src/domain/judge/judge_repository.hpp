#pragma once

#include "db/connection_pool.hpp"
#include "domain/judge/judge_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace judge {

class JudgeRepository final {
public:
    explicit JudgeRepository(ConnectionPool& pool);

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
    bool resetSubmissionForRejudge(long long id) const;

    void insertSubmissionResult(const SubmissionResult& result) const;
    void completeSubmission(
        long long id,
        const std::string& verdict,
        int score,
        const std::string& compile_stdout,
        const std::string& compile_stderr,
        std::optional<int> compile_time_ms,
        std::optional<int> max_time_ms,
        std::optional<int> max_memory_kb
    ) const;
    void failSubmission(
        long long id,
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
