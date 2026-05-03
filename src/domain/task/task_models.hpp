#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace task {

struct Task {
    long long id{};
    std::string type;
    std::string status;
    std::optional<long long> owner_user_id;
    nlohmann::json payload = nlohmann::json::object();
    std::optional<nlohmann::json> result;
    std::optional<std::string> error_message;
    std::string created_at;
    std::optional<std::string> started_at;
    std::optional<std::string> finished_at;
};

inline void to_json(nlohmann::json& json, const Task& task) {
    json = {
        {"id", task.id},
        {"taskId", task.id},
        {"type", task.type},
        {"status", task.status},
        {"ownerUserId", task.owner_user_id},
        {"result", task.result},
        {"error", task.error_message},
        {"createdAt", task.created_at},
        {"startedAt", task.started_at},
        {"finishedAt", task.finished_at}
    };
}

} // namespace task
