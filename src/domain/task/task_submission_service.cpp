#include "domain/task/task_submission_service.hpp"

namespace task {

TaskSubmissionService::TaskSubmissionService(TaskRepository& repository)
    : repository_(repository) {}

long long TaskSubmissionService::submit(
    const std::string& type,
    std::optional<long long> owner_user_id,
    const nlohmann::json& payload
) const {
    return repository_.createTask(type, owner_user_id, payload);
}

} // namespace task
