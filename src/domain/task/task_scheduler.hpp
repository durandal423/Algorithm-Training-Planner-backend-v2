#pragma once

#include "domain/task/task_repository.hpp"
#include "concurrency/thread_pool.hpp"

#include <nlohmann/json.hpp>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>

namespace task {

class TaskQueueFull final : public std::runtime_error {
public:
    TaskQueueFull();
};

class TaskScheduler final {
public:
    using Handler = std::function<nlohmann::json(long long task_id)>;

    TaskScheduler(TaskRepository& repository, concurrency::ThreadPool& pool);

    long long submit(
        const std::string& type,
        std::optional<long long> owner_user_id,
        const nlohmann::json& payload,
        Handler handler
    ) const;

private:
    TaskRepository& repository_;
    concurrency::ThreadPool& pool_;
};

} // namespace task
