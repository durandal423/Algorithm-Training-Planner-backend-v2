#pragma once

#include "db/connection_pool.hpp"
#include "domain/task/task_models.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace task {

namespace detail {

template <typename Field>
nlohmann::json json_or_empty_object(const Field& field) {
    return nlohmann::json::parse(field.template as<std::string>("{}"));
}

template <typename Field>
std::optional<nlohmann::json> optional_json(const Field& field) {
    const auto text = field.template get<std::string>();
    if (!text) {
        return std::nullopt;
    }
    return nlohmann::json::parse(*text);
}

} // namespace detail

template <typename Row>
Task task_from_row(const Row& row) {
    return Task{
        .id = row["id"].template as<long long>(),
        .type = row["type"].template as<std::string>(),
        .status = row["status"].template as<std::string>(),
        .owner_user_id = row["owner_user_id"].template get<long long>(),
        .payload = detail::json_or_empty_object(row["payload"]),
        .result = detail::optional_json(row["result"]),
        .error_message = row["error_message"].template get<std::string>(),
        .created_at = row["created_at"].template as<std::string>(),
        .started_at = row["started_at"].template get<std::string>(),
        .finished_at = row["finished_at"].template get<std::string>()
    };
}

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
    std::optional<Task> getAuthTaskByToken(
        long long id,
        const std::string& task_token
    ) const;

    void markRunning(long long id) const;
    void markSucceeded(long long id, const nlohmann::json& result) const;
    void markFailed(long long id, const std::string& error_message) const;

private:
    ConnectionPool& pool_;
};

} // namespace task
