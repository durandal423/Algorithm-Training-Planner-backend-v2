#pragma once

#include "db/connection_pool.hpp"
#include "domain/task/task_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace task {

class TaskQueueRepository final {
public:
    explicit TaskQueueRepository(ConnectionPool& pool);

    std::optional<Task> claimNextPendingTask(
        const std::string& worker_id,
        const std::vector<std::string>& types
    ) const;

    void markSucceeded(long long id, const nlohmann::json& result) const;
    void markFailed(long long id, const std::string& error_message) const;

    int recoverStaleRunningTasks(int stale_after_seconds) const;

private:
    ConnectionPool& pool_;
};

} // namespace task
