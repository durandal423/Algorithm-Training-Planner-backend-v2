#include "domain/task/task_scheduler.hpp"

#include <spdlog/spdlog.h>

#include <utility>

namespace task {

TaskQueueFull::TaskQueueFull()
    : std::runtime_error{"background task queue is full"} {}

TaskScheduler::TaskScheduler(
    TaskRepository& repository,
    concurrency::ThreadPool& pool
)
    : repository_(repository),
      pool_(pool) {}

long long TaskScheduler::submit(
    const std::string& type,
    std::optional<long long> owner_user_id,
    const nlohmann::json& payload,
    Handler handler
) const {
    const auto task_id = repository_.createTask(type, owner_user_id, payload);

    const bool accepted = pool_.trySubmit([this, task_id, handler = std::move(handler)] {
        try {
            repository_.markRunning(task_id);
            const auto result = handler(task_id);
            repository_.markSucceeded(task_id, result);
        } catch (const std::exception& error) {
            repository_.markFailed(task_id, error.what());
            spdlog::error("background task {} failed: {}", task_id, error.what());
        } catch (...) {
            repository_.markFailed(task_id, "unknown background task error");
            spdlog::error("background task {} failed: unknown error", task_id);
        }
    });

    if (!accepted) {
        repository_.markFailed(task_id, "background task queue is full");
        throw TaskQueueFull{};
    }

    return task_id;
}

} // namespace task
