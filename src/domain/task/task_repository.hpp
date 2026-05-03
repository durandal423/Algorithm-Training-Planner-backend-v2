#pragma once

#include "db/connection_pool.hpp"
#include "domain/task/task_models.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace task {

class TaskRepository final {
public:
    explicit TaskRepository(ConnectionPool& pool);

    long long createTask(
        const std::string& type,
        std::optional<long long> owner_user_id,
        const nlohmann::json& payload
    ) const;

    std::optional<Task> getTask(
        long long id,
        std::optional<long long> owner_user_id
    ) const;

    void markRunning(long long id) const;
    void markSucceeded(long long id, const nlohmann::json& result) const;
    void markFailed(long long id, const std::string& error_message) const;

private:
    ConnectionPool& pool_;
};

} // namespace task
