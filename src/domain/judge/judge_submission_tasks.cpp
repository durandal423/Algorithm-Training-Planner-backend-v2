#include "domain/judge/judge_repository.hpp"
#include "domain/task/task_handler_registry.hpp"
#include "domain/task/task_models.hpp"

#include "config.hpp"

#include <stdexcept>
#include <string>

namespace {

nlohmann::json run_judge_submission_create(
    task::TaskExecutionContext& context,
    const task::Task& task
) {
    if (!task.owner_user_id) {
        throw std::runtime_error{"judge submission task is missing owner_user_id"};
    }

    judge::Submission submission;
    submission.problem_id = task.payload.at("problemId").get<long long>();
    submission.user_id = *task.owner_user_id;
    submission.language = task.payload.value("language", std::string{"cpp"});
    submission.source_code = task.payload.at("sourceCode").get<std::string>();

    judge::JudgeRepository repository{context.apiPool()};
    const auto id = repository.createSubmission(submission);
    return nlohmann::json{
        {"submissionTaskId", task::task_ref_for_type("judge.submission.create", task.id)},
        {"id", id},
        {"status", "queued"}
    };
}

const task::TaskHandlerRegistration kJudgeSubmissionCreateTask{
    task::TaskHandlerSpec{
        .type = "judge.submission.create",
        .pool_name = "judge-create",
        .worker_count = config::TASK_WORKER_JUDGE_CREATE_THREAD_COUNT,
        .handler = run_judge_submission_create
    }
};

} // namespace
