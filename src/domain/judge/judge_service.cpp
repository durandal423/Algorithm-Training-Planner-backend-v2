#include "domain/judge/judge_service.hpp"

#include "config.hpp"
#include "domain/judge/output_comparator.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace judge {
namespace {

std::filesystem::path make_work_dir(long long submission_id) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::path{config::JUDGE_DOCKER_WORK_ROOT} /
        ("atp-judge-" + std::to_string(submission_id) + "-" + std::to_string(stamp));
    std::filesystem::create_directories(path);
    return path;
}

SubmissionResult make_result(
    const Submission& submission,
    const JudgeTestCase& test_case,
    const std::string& verdict,
    const RunResult& run,
    std::string message
) {
    return SubmissionResult{
        .submission_id = submission.id,
        .test_case_id = test_case.id,
        .order_index = test_case.order_index,
        .status = "completed",
        .verdict = verdict,
        .time_ms = run.time_ms,
        .memory_kb = run.memory_kb,
        .exit_code = run.exit_code,
        .stdout_sample = run.stdout_sample,
        .stderr_sample = run.stderr_sample,
        .message = std::move(message)
    };
}

} // namespace

JudgeService::JudgeService(
    JudgeRepository& repository,
    SandboxRunner runner,
    std::string worker_id
)
    : repository_(repository),
      runner_(std::move(runner)),
      worker_id_(std::move(worker_id)) {}

bool JudgeService::processNextSubmission() {
    auto submission = repository_.fetchNextQueuedSubmission(worker_id_);
    if (!submission) {
        return false;
    }

    const auto work_dir = make_work_dir(submission->id);
    spdlog::info(
        "judge submission picked up (submission_id={}, problem_id={}, worker={})",
        submission->id,
        submission->problem_id,
        worker_id_
    );

    try {
        const auto config = repository_.getConfig(submission->problem_id);
        if (!config || !config->enabled) {
            repository_.failSubmission(
                submission->id,
                worker_id_,
                "JE",
                "judge config is missing or disabled"
            );
            std::filesystem::remove_all(work_dir);
            return true;
        }

        const auto test_cases = repository_.listJudgingTestCases(submission->problem_id);
        if (test_cases.empty()) {
            repository_.failSubmission(submission->id, worker_id_, "JE", "no judge test cases configured");
            std::filesystem::remove_all(work_dir);
            return true;
        }

        const auto compile = runner_.compile(*submission, *config, work_dir);
        if (!compile.success) {
            const auto verdict = compile.system_error ? "JE" : "CE";
            repository_.completeSubmission(
                submission->id,
                worker_id_,
                verdict,
                0,
                compile.stdout_sample,
                compile.stderr_sample,
                compile.time_ms,
                std::nullopt,
                std::nullopt
            );
            std::filesystem::remove_all(work_dir);
            return true;
        }
        if (!repository_.refreshSubmissionLease(submission->id, worker_id_)) {
            spdlog::warn(
                "judge submission lease lost after compile (submission_id={}, worker={})",
                submission->id,
                worker_id_
            );
            std::filesystem::remove_all(work_dir);
            return true;
        }

        std::string final_verdict = "AC";
        int score = 0;
        int max_time_ms = 0;
        int max_memory_kb = 0;
        int run_index = 1;

        for (const auto& test_case : test_cases) {
            if (!test_case.expected_output) {
                SubmissionResult result;
                result.submission_id = submission->id;
                result.test_case_id = test_case.id;
                result.order_index = test_case.order_index;
                result.status = "failed";
                result.verdict = "JE";
                result.message = "test case has no expected output";
                if (!repository_.insertSubmissionResult(result, worker_id_)) {
                    spdlog::warn(
                        "judge submission lease lost while recording result (submission_id={}, worker={})",
                        submission->id,
                        worker_id_
                    );
                    std::filesystem::remove_all(work_dir);
                    return true;
                }
                final_verdict = "JE";
                break;
            }

            if (!repository_.refreshSubmissionLease(submission->id, worker_id_)) {
                spdlog::warn(
                    "judge submission lease lost before test case run (submission_id={}, worker={})",
                    submission->id,
                    worker_id_
                );
                std::filesystem::remove_all(work_dir);
                return true;
            }
            const auto run = runner_.run(*config, test_case, work_dir, run_index++);
            if (!repository_.refreshSubmissionLease(submission->id, worker_id_)) {
                spdlog::warn(
                    "judge submission lease lost after test case run (submission_id={}, worker={})",
                    submission->id,
                    worker_id_
                );
                std::filesystem::remove_all(work_dir);
                return true;
            }
            max_time_ms = std::max(max_time_ms, run.time_ms);
            max_memory_kb = std::max(max_memory_kb, run.memory_kb);

            std::string verdict = "AC";
            std::string message = "accepted";
            if (run.system_error) {
                verdict = "JE";
                message = run.message.empty() ? "judge runner failed during execution" : run.message;
            } else if (run.timed_out) {
                verdict = "TLE";
                message = "time limit exceeded";
            } else if (run.output_limit_exceeded) {
                verdict = "OLE";
                message = "output limit exceeded";
            } else if (run.exit_code != 0) {
                verdict = "RE";
                message = "runtime error";
            } else {
                const auto comparison = compareOutputs(
                    *test_case.expected_output,
                    run.stdout_sample,
                    config->compare_mode,
                    config->float_epsilon
                );
                if (!comparison.accepted) {
                    verdict = "WA";
                    message = comparison.message;
                }
            }

            if (!repository_.insertSubmissionResult(
                    make_result(*submission, test_case, verdict, run, message),
                    worker_id_
                )) {
                spdlog::warn(
                    "judge submission lease lost while recording result (submission_id={}, worker={})",
                    submission->id,
                    worker_id_
                );
                std::filesystem::remove_all(work_dir);
                return true;
            }
            if (verdict == "AC") {
                score += test_case.points;
            } else {
                final_verdict = verdict;
                break;
            }
        }

        if (!repository_.completeSubmission(
            submission->id,
            worker_id_,
            final_verdict,
            score,
            compile.stdout_sample,
            compile.stderr_sample,
            compile.time_ms,
            max_time_ms,
            max_memory_kb
        )) {
            spdlog::warn(
                "judge submission lease lost while completing submission (submission_id={}, worker={})",
                submission->id,
                worker_id_
            );
            std::filesystem::remove_all(work_dir);
            return true;
        }
        std::filesystem::remove_all(work_dir);
        spdlog::info(
            "judge submission completed (submission_id={}, verdict={}, score={})",
            submission->id,
            final_verdict,
            score
        );
        return true;
    } catch (const std::exception& error) {
        repository_.failSubmission(submission->id, worker_id_, "JE", error.what());
        std::filesystem::remove_all(work_dir);
        spdlog::error("judge submission failed (submission_id={}): {}", submission->id, error.what());
        return true;
    }
}

} // namespace judge
