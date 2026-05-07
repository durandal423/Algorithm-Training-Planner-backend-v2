#include "domain/judge/judge_repository.hpp"

#include <pqxx/pqxx>

#include <algorithm>

namespace judge {
namespace {

constexpr auto kSubmissionSelect = R"SQL(
    SELECT id, problem_id, user_id, language, source_code, status, verdict, score,
           compile_stdout, compile_stderr, compile_time_ms, max_time_ms, max_memory_kb,
           submitted_at, started_at, finished_at
    FROM submissions
)SQL";

constexpr auto kSubmissionSummarySelect = R"SQL(
    SELECT id, problem_id, user_id, language, status, verdict, score,
           compile_stdout, compile_stderr, compile_time_ms, max_time_ms, max_memory_kb,
           submitted_at, started_at, finished_at
    FROM submissions
)SQL";

} // namespace

JudgeRepository::JudgeRepository(ConnectionPool& pool)
    : pool_(pool) {}

std::optional<long long> JudgeRepository::findProblemIdByCode(const std::string& problem_code) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        R"SQL(
            SELECT id
            FROM problems
            WHERE code = $1
        )SQL",
        pqxx::params{problem_code}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return row.value()["id"].as<long long>();
}

std::optional<JudgeConfig> JudgeRepository::getConfig(long long problem_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        R"SQL(
            SELECT id, problem_id, enabled, language, compile_command_template,
                   run_command_template, time_limit_ms, memory_limit_mb,
                   output_limit_kb, compare_mode, float_epsilon
            FROM judge_configs
            WHERE problem_id = $1
        )SQL",
        pqxx::params{problem_id}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return config_from_row(row.value());
}

JudgeConfig JudgeRepository::upsertConfig(const JudgeConfig& config) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        R"SQL(
            INSERT INTO judge_configs(
                problem_id, enabled, language, compile_command_template, run_command_template,
                time_limit_ms, memory_limit_mb, output_limit_kb, compare_mode, float_epsilon
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10)
            ON CONFLICT (problem_id) DO UPDATE
            SET enabled = EXCLUDED.enabled,
                language = EXCLUDED.language,
                compile_command_template = EXCLUDED.compile_command_template,
                run_command_template = EXCLUDED.run_command_template,
                time_limit_ms = EXCLUDED.time_limit_ms,
                memory_limit_mb = EXCLUDED.memory_limit_mb,
                output_limit_kb = EXCLUDED.output_limit_kb,
                compare_mode = EXCLUDED.compare_mode,
                float_epsilon = EXCLUDED.float_epsilon
            RETURNING id, problem_id, enabled, language, compile_command_template,
                      run_command_template, time_limit_ms, memory_limit_mb,
                      output_limit_kb, compare_mode, float_epsilon
        )SQL",
        pqxx::params{
            config.problem_id,
            config.enabled,
            config.language,
            config.compile_command_template,
            config.run_command_template,
            config.time_limit_ms,
            config.memory_limit_mb,
            config.output_limit_kb,
            config.compare_mode,
            config.float_epsilon
        }
    ).opt_row();
    tx.commit();
    return config_from_row(row.value());
}

std::vector<JudgeTestCase> JudgeRepository::listJudgingTestCases(long long problem_id) const {
    return listTestCases(problem_id, true);
}

std::vector<JudgeTestCase> JudgeRepository::listTestCases(long long problem_id, bool include_hidden) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<JudgeTestCase> cases;
    const auto result= tx.exec(
        R"SQL(
            SELECT id, problem_id, name, input_data, expected_output, visibility,
                   points, order_index, time_limit_ms, memory_limit_mb
            FROM judge_test_cases
            WHERE problem_id = $1
              AND ($2 OR visibility = 'sample')
            ORDER BY order_index, id
        )SQL",
        pqxx::params{problem_id, include_hidden}
    );
    for (const pqxx::row_ref row : result) {
        cases.push_back(test_case_from_row(row));
    }
    tx.commit();
    return cases;
}

long long JudgeRepository::createTestCase(const JudgeTestCase& test_case) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO judge_test_cases(
                problem_id, name, input_data, expected_output, visibility,
                points, order_index, time_limit_ms, memory_limit_mb
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)
            RETURNING id
        )SQL",
        pqxx::params{
            test_case.problem_id,
            test_case.name,
            test_case.input_data,
            test_case.expected_output,
            test_case.visibility,
            test_case.points,
            test_case.order_index,
            test_case.time_limit_ms,
            test_case.memory_limit_mb
        }
    ).one_field().as<long long>();
    tx.commit();
    return id;
}

std::optional<JudgeTestCase> JudgeRepository::getTestCase(long long id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        R"SQL(
            SELECT id, problem_id, name, input_data, expected_output, visibility,
                   points, order_index, time_limit_ms, memory_limit_mb
            FROM judge_test_cases
            WHERE id = $1
        )SQL",
        pqxx::params{id}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return test_case_from_row(row.value());
}

std::optional<JudgeTestCase> JudgeRepository::updateTestCase(
    long long id,
    const JudgeTestCase& test_case
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        R"SQL(
            UPDATE judge_test_cases
            SET name = $2,
                input_data = $3,
                expected_output = $4,
                visibility = $5,
                points = $6,
                order_index = $7,
                time_limit_ms = $8,
                memory_limit_mb = $9
            WHERE id = $1
            RETURNING id, problem_id, name, input_data, expected_output, visibility,
                      points, order_index, time_limit_ms, memory_limit_mb
        )SQL",
        pqxx::params{
            id,
            test_case.name,
            test_case.input_data,
            test_case.expected_output,
            test_case.visibility,
            test_case.points,
            test_case.order_index,
            test_case.time_limit_ms,
            test_case.memory_limit_mb
        }
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return test_case_from_row(row.value());
}

bool JudgeRepository::deleteTestCase(long long id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        "DELETE FROM judge_test_cases WHERE id = $1",
        pqxx::params{id}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

long long JudgeRepository::createSubmission(const Submission& submission) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO submissions(problem_id, user_id, language, source_code)
            VALUES ($1, $2, $3, $4)
            RETURNING id
        )SQL",
        pqxx::params{
            submission.problem_id,
            submission.user_id,
            submission.language,
            submission.source_code
        }
    ).one_field().as<long long>();
    tx.commit();
    return id;
}

std::optional<Submission> JudgeRepository::getSubmission(long long id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kSubmissionSelect} + " WHERE id = $1",
        pqxx::params{id}
    ).opt_row();
    if (!row) {
        tx.commit();
        return std::nullopt;
    }
    auto submission = submission_from_row(row.value());
    submission.results = loadResults(tx, submission.id);
    tx.commit();
    return submission;
}

std::vector<Submission> JudgeRepository::listSubmissions(const SubmissionFilter& filter) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<Submission> submissions;
    const int limit = std::clamp(filter.limit, 1, 200);
    const int offset = std::max(0, filter.offset);
    const auto result= tx.exec(
        std::string{kSubmissionSummarySelect} + R"SQL(
            WHERE ($1::bigint IS NULL OR problem_id = $1)
              AND ($2::bigint IS NULL OR user_id = $2)
              AND ($3 = '' OR status = $3)
              AND ($4 = '' OR verdict = $4)
            ORDER BY submitted_at DESC, id DESC
            LIMIT $5 OFFSET $6
        )SQL",
        pqxx::params{
            filter.problem_id,
            filter.user_id,
            filter.status,
            filter.verdict,
            limit,
            offset
        }
    );
    for (const pqxx::row_ref row : result) {
        submissions.push_back(submission_summary_from_row(row));
    }
    tx.commit();
    return submissions;
}

std::optional<Submission> JudgeRepository::fetchNextQueuedSubmission(const std::string& worker_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        R"SQL(
            WITH picked AS (
                SELECT id
                FROM submissions
                WHERE status = 'queued'
                ORDER BY submitted_at, id
                FOR UPDATE SKIP LOCKED
                LIMIT 1
            )
            UPDATE submissions s
            SET status = 'judging',
                verdict = NULL,
                score = 0,
                compile_stdout = NULL,
                compile_stderr = NULL,
                compile_time_ms = NULL,
                max_time_ms = NULL,
                max_memory_kb = NULL,
                locked_by = $1,
                locked_at = now(),
                attempt_count = attempt_count + 1,
                started_at = now(),
                finished_at = NULL
            FROM picked
            WHERE s.id = picked.id
            RETURNING s.id, s.problem_id, s.user_id, s.language, s.source_code, s.status,
                      s.verdict, s.score, s.compile_stdout, s.compile_stderr,
                      s.compile_time_ms, s.max_time_ms, s.max_memory_kb,
                      s.submitted_at, s.started_at, s.finished_at
        )SQL",
        pqxx::params{worker_id}
    ).opt_row();
    if (!row) {
        tx.commit();
        return std::nullopt;
    }
    auto submission = submission_from_row(row.value());
    tx.exec(
        "DELETE FROM submission_results WHERE submission_id = $1",
        pqxx::params{submission.id}
    ).no_rows();
    tx.commit();
    return submission;
}

bool JudgeRepository::refreshSubmissionLease(long long id, const std::string& worker_id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result = tx.exec(
        R"SQL(
            UPDATE submissions
            SET locked_at = now()
            WHERE id = $1
              AND status = 'judging'
              AND locked_by = $2
        )SQL",
        pqxx::params{id, worker_id}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

int JudgeRepository::recoverStaleJudgingSubmissions(
    int stale_after_seconds,
    int max_attempts
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto result= tx.exec(
        R"SQL(
            UPDATE submissions
            SET status = CASE WHEN attempt_count >= $2 THEN 'failed' ELSE 'queued' END,
                verdict = CASE WHEN attempt_count >= $2 THEN 'JE' ELSE NULL END,
                score = 0,
                compile_stdout = NULL,
                compile_stderr = CASE
                    WHEN attempt_count >= $2 THEN 'judge worker lease expired'
                    ELSE NULL
                END,
                compile_time_ms = NULL,
                max_time_ms = NULL,
                max_memory_kb = NULL,
                locked_by = NULL,
                locked_at = NULL,
                started_at = CASE WHEN attempt_count >= $2 THEN started_at ELSE NULL END,
                finished_at = CASE WHEN attempt_count >= $2 THEN now() ELSE NULL END
            WHERE status = 'judging'
              AND locked_at IS NOT NULL
              AND locked_at < now() - ($1::integer * interval '1 second')
            RETURNING id, status, verdict
        )SQL",
        pqxx::params{stale_after_seconds, max_attempts}
    );
    for (const pqxx::row_ref row : result) {
        if (row["status"].as<std::string>() == "failed") {
            updateLatestPlanItemFromSubmission(tx, row["id"].as<long long>(), row["verdict"].as<std::string>());
        }
    }
    tx.commit();
    return static_cast<int>(result.size());
}

bool JudgeRepository::resetSubmissionForRejudge(long long id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        "DELETE FROM submission_results WHERE submission_id = $1",
        pqxx::params{id}
    ).no_rows();
    const auto result = tx.exec(
        R"SQL(
            UPDATE submissions
            SET status = 'queued',
                verdict = NULL,
                score = 0,
                compile_stdout = NULL,
                compile_stderr = NULL,
                compile_time_ms = NULL,
                max_time_ms = NULL,
                max_memory_kb = NULL,
                locked_by = NULL,
                locked_at = NULL,
                started_at = NULL,
                finished_at = NULL
            WHERE id = $1
              AND status IN ('completed', 'failed', 'canceled')
        )SQL",
        pqxx::params{id}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

bool JudgeRepository::insertSubmissionResult(
    const SubmissionResult& result,
    const std::string& worker_id
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto inserted = tx.exec(
        R"SQL(
            INSERT INTO submission_results(
                submission_id, test_case_id, order_index, status, verdict,
                time_ms, memory_kb, exit_code, stdout_sample, stderr_sample, message
            )
            SELECT $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11
            WHERE EXISTS (
                SELECT 1
                FROM submissions
                WHERE id = $1
                  AND status = 'judging'
                  AND locked_by = $12
            )
            RETURNING id
        )SQL",
        pqxx::params{
            result.submission_id,
            result.test_case_id,
            result.order_index,
            result.status,
            result.verdict,
            result.time_ms,
            result.memory_kb,
            result.exit_code,
            result.stdout_sample,
            result.stderr_sample,
            result.message,
            worker_id
        }
    );
    tx.commit();
    return !inserted.empty();
}

bool JudgeRepository::completeSubmission(
    long long id,
    const std::string& worker_id,
    const std::string& verdict,
    int score,
    const std::string& compile_stdout,
    const std::string& compile_stderr,
    std::optional<int> compile_time_ms,
    std::optional<int> max_time_ms,
    std::optional<int> max_memory_kb
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto updated = tx.exec(
        R"SQL(
            UPDATE submissions
            SET status = 'completed',
                verdict = $3,
                score = $4,
                compile_stdout = $5,
                compile_stderr = $6,
                compile_time_ms = $7,
                max_time_ms = $8,
                max_memory_kb = $9,
                locked_by = NULL,
                locked_at = NULL,
                finished_at = now()
            WHERE id = $1
              AND status = 'judging'
              AND locked_by = $2
            RETURNING id
        )SQL",
        pqxx::params{
            id,
            worker_id,
            verdict,
            score,
            compile_stdout,
            compile_stderr,
            compile_time_ms,
            max_time_ms,
            max_memory_kb
        }
    );
    if (updated.empty()) {
        tx.commit();
        return false;
    }
    updateLatestPlanItemFromSubmission(tx, id, verdict);
    tx.commit();
    return true;
}

bool JudgeRepository::failSubmission(
    long long id,
    const std::string& worker_id,
    const std::string& verdict,
    const std::string& message,
    const std::string& compile_stdout,
    const std::string& compile_stderr
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto updated = tx.exec(
        R"SQL(
            UPDATE submissions
            SET status = 'failed',
                verdict = $3,
                score = 0,
                compile_stdout = $4,
                compile_stderr = $5,
                locked_by = NULL,
                locked_at = NULL,
                finished_at = now()
            WHERE id = $1
              AND status = 'judging'
              AND locked_by = $2
            RETURNING id
        )SQL",
        pqxx::params{
            id,
            worker_id,
            verdict,
            compile_stdout,
            compile_stderr.empty() ? message : compile_stderr
        }
    );
    if (updated.empty()) {
        tx.commit();
        return false;
    }
    updateLatestPlanItemFromSubmission(tx, id, verdict);
    tx.commit();
    return true;
}

std::vector<SubmissionResult> JudgeRepository::loadResults(
    pqxx::transaction_base& tx,
    long long submission_id
) const {
    std::vector<SubmissionResult> results;
    const auto result= tx.exec(
        R"SQL(
            SELECT id, submission_id, test_case_id, order_index, status, verdict,
                   time_ms, memory_kb, exit_code, stdout_sample, stderr_sample, message
            FROM submission_results
            WHERE submission_id = $1
            ORDER BY order_index, id
        )SQL",
        pqxx::params{submission_id}
    );
    for (const pqxx::row_ref row : result) {
        results.push_back(result_from_row(row));
    }
    return results;
}

void JudgeRepository::updateLatestPlanItemFromSubmission(
    pqxx::transaction_base& tx,
    long long submission_id,
    const std::string& verdict
) const {
    tx.exec(
        R"SQL(
            WITH target AS (
                SELECT tpi.id
                FROM submissions s
                JOIN training_plans tp
                  ON tp.user_id = s.user_id
                 AND tp.status IN ('active', 'draft')
                JOIN training_plan_items tpi
                  ON tpi.plan_id = tp.id
                 AND tpi.problem_id = s.problem_id
                WHERE s.id = $1
                ORDER BY tp.created_at DESC, tpi.order_index DESC, tpi.id DESC
                LIMIT 1
            )
            UPDATE training_plan_items tpi
            SET last_submission_id = $1,
                last_verdict = $2::varchar,
                status = CASE WHEN $2::text = 'AC' THEN 'completed' ELSE 'failed' END
            FROM target
            WHERE tpi.id = target.id
        )SQL",
        pqxx::params{submission_id, verdict}
    ).no_rows();
}

} // namespace judge
