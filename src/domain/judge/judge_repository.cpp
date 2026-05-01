#include "domain/judge/judge_repository.hpp"

#include <pqxx/pqxx>

#include <algorithm>

namespace judge {
namespace {

std::optional<std::string> optional_string(const pqxx::field& field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<std::string>();
}

std::optional<int> optional_int(const pqxx::field& field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<int>();
}

std::optional<long long> optional_long(const pqxx::field& field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<long long>();
}

std::string string_or_empty(const pqxx::field& field) {
    if (field.is_null()) {
        return {};
    }
    return field.as<std::string>();
}

JudgeConfig config_from_row(const pqxx::row& row) {
    JudgeConfig config;
    config.id = row["id"].as<long long>();
    config.problem_id = row["problem_id"].as<long long>();
    config.enabled = row["enabled"].as<bool>();
    config.language = row["language"].as<std::string>();
    config.compile_command_template = optional_string(row["compile_command_template"]);
    config.run_command_template = optional_string(row["run_command_template"]);
    config.time_limit_ms = row["time_limit_ms"].as<int>();
    config.memory_limit_mb = row["memory_limit_mb"].as<int>();
    config.output_limit_kb = row["output_limit_kb"].as<int>();
    config.compare_mode = row["compare_mode"].as<std::string>();
    config.float_epsilon = row["float_epsilon"].as<double>();
    return config;
}

JudgeTestCase test_case_from_row(const pqxx::row& row) {
    JudgeTestCase test_case;
    test_case.id = row["id"].as<long long>();
    test_case.problem_id = row["problem_id"].as<long long>();
    test_case.name = string_or_empty(row["name"]);
    test_case.input_data = row["input_data"].as<std::string>();
    test_case.expected_output = optional_string(row["expected_output"]);
    test_case.visibility = row["visibility"].as<std::string>();
    test_case.points = row["points"].as<int>();
    test_case.order_index = row["order_index"].as<int>();
    test_case.time_limit_ms = optional_int(row["time_limit_ms"]);
    test_case.memory_limit_mb = optional_int(row["memory_limit_mb"]);
    return test_case;
}

Submission submission_from_row(const pqxx::row& row) {
    Submission submission;
    submission.id = row["id"].as<long long>();
    submission.problem_id = row["problem_id"].as<long long>();
    submission.user_id = optional_long(row["user_id"]);
    submission.language = row["language"].as<std::string>();
    submission.source_code = row["source_code"].as<std::string>();
    submission.status = row["status"].as<std::string>();
    submission.verdict = optional_string(row["verdict"]);
    submission.score = row["score"].as<int>();
    submission.compile_stdout = string_or_empty(row["compile_stdout"]);
    submission.compile_stderr = string_or_empty(row["compile_stderr"]);
    submission.compile_time_ms = optional_int(row["compile_time_ms"]);
    submission.max_time_ms = optional_int(row["max_time_ms"]);
    submission.max_memory_kb = optional_int(row["max_memory_kb"]);
    submission.submitted_at = string_or_empty(row["submitted_at"]);
    submission.started_at = optional_string(row["started_at"]);
    submission.finished_at = optional_string(row["finished_at"]);
    return submission;
}

SubmissionResult result_from_row(const pqxx::row& row) {
    SubmissionResult result;
    result.id = row["id"].as<long long>();
    result.submission_id = row["submission_id"].as<long long>();
    result.test_case_id = optional_long(row["test_case_id"]);
    result.order_index = row["order_index"].as<int>();
    result.status = row["status"].as<std::string>();
    result.verdict = optional_string(row["verdict"]);
    result.time_ms = optional_int(row["time_ms"]);
    result.memory_kb = optional_int(row["memory_kb"]);
    result.exit_code = optional_int(row["exit_code"]);
    result.stdout_sample = string_or_empty(row["stdout_sample"]);
    result.stderr_sample = string_or_empty(row["stderr_sample"]);
    result.message = string_or_empty(row["message"]);
    return result;
}

constexpr auto kSubmissionSelect = R"SQL(
    SELECT id, problem_id, user_id, language, source_code, status, verdict, score,
           compile_stdout, compile_stderr, compile_time_ms, max_time_ms, max_memory_kb,
           submitted_at, started_at, finished_at
    FROM submissions
)SQL";

} // namespace

JudgeRepository::JudgeRepository(ConnectionPool& pool)
    : pool_(pool) {}

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
    return config_from_row(*row);
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
    ).one_row();
    tx.commit();
    return config_from_row(row);
}

std::vector<JudgeTestCase> JudgeRepository::listJudgingTestCases(long long problem_id) const {
    return listTestCases(problem_id, true);
}

std::vector<JudgeTestCase> JudgeRepository::listTestCases(long long problem_id, bool include_hidden) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<JudgeTestCase> cases;
    const auto rows = tx.exec(
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
    for (const auto& row : rows) {
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
    return test_case_from_row(*row);
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
    return test_case_from_row(*row);
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
    auto submission = submission_from_row(*row);
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
    const auto rows = tx.exec(
        std::string{kSubmissionSelect} + R"SQL(
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
    for (const auto& row : rows) {
        submissions.push_back(submission_from_row(row));
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
    auto submission = submission_from_row(*row);
    tx.exec(
        "DELETE FROM submission_results WHERE submission_id = $1",
        pqxx::params{submission.id}
    ).no_rows();
    tx.commit();
    return submission;
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
        )SQL",
        pqxx::params{id}
    );
    tx.commit();
    return result.affected_rows() > 0;
}

void JudgeRepository::insertSubmissionResult(const SubmissionResult& result) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        R"SQL(
            INSERT INTO submission_results(
                submission_id, test_case_id, order_index, status, verdict,
                time_ms, memory_kb, exit_code, stdout_sample, stderr_sample, message
            )
            VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
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
            result.message
        }
    ).no_rows();
    tx.commit();
}

void JudgeRepository::completeSubmission(
    long long id,
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
    tx.exec(
        R"SQL(
            UPDATE submissions
            SET status = 'completed',
                verdict = $2,
                score = $3,
                compile_stdout = $4,
                compile_stderr = $5,
                compile_time_ms = $6,
                max_time_ms = $7,
                max_memory_kb = $8,
                locked_by = NULL,
                locked_at = NULL,
                finished_at = now()
            WHERE id = $1
        )SQL",
        pqxx::params{
            id,
            verdict,
            score,
            compile_stdout,
            compile_stderr,
            compile_time_ms,
            max_time_ms,
            max_memory_kb
        }
    ).no_rows();
    updateLatestPlanItemFromSubmission(tx, id, verdict);
    tx.commit();
}

void JudgeRepository::failSubmission(
    long long id,
    const std::string& verdict,
    const std::string& message,
    const std::string& compile_stdout,
    const std::string& compile_stderr
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        R"SQL(
            UPDATE submissions
            SET status = 'failed',
                verdict = $2,
                score = 0,
                compile_stdout = $3,
                compile_stderr = $4,
                locked_by = NULL,
                locked_at = NULL,
                finished_at = now()
            WHERE id = $1
        )SQL",
        pqxx::params{
            id,
            verdict,
            compile_stdout,
            compile_stderr.empty() ? message : compile_stderr
        }
    ).no_rows();
    updateLatestPlanItemFromSubmission(tx, id, verdict);
    tx.commit();
}

std::vector<SubmissionResult> JudgeRepository::loadResults(
    pqxx::transaction_base& tx,
    long long submission_id
) const {
    std::vector<SubmissionResult> results;
    const auto rows = tx.exec(
        R"SQL(
            SELECT id, submission_id, test_case_id, order_index, status, verdict,
                   time_ms, memory_kb, exit_code, stdout_sample, stderr_sample, message
            FROM submission_results
            WHERE submission_id = $1
            ORDER BY order_index, id
        )SQL",
        pqxx::params{submission_id}
    );
    for (const auto& row : rows) {
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
