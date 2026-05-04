#pragma once

#include "domain/task/task_repository.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace task {

class TaskSubmissionService final {
public:
    explicit TaskSubmissionService(TaskRepository& repository);

    long long submit(
        const std::string& type,
        std::optional<long long> owner_user_id,
        const nlohmann::json& payload
    ) const;

private:
    TaskRepository& repository_;
};

} // namespace task
