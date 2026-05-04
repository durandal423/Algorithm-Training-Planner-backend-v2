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

inline std::string task_ref_for_type(const std::string& type, long long id) {
    if (type.rfind("auth.", 0) == 0) {
        return "auth_" + std::to_string(id);
    }
    if (type.rfind("training.", 0) == 0) {
        return "training_" + std::to_string(id);
    }
    if (type.rfind("judge.submission.", 0) == 0) {
        return "submission_" + std::to_string(id);
    }
    return "task_" + std::to_string(id);
}

inline void to_json(nlohmann::json& json, const Task& task) {
    json = {
        {"id", task.id},
        {"taskRef", task_ref_for_type(task.type, task.id)},
        {"type", task.type},
        {"status", task.status},
        {"ownerUserId", task.owner_user_id},
        {"result", task.result},
        {"error", task.error_message},
        {"createdAt", task.created_at},
        {"startedAt", task.started_at},
        {"finishedAt", task.finished_at}
    };

    if (task.type.rfind("auth.", 0) == 0) {
        json["authTaskId"] = task_ref_for_type(task.type, task.id);
    } else if (task.type.rfind("training.", 0) == 0) {
        json["trainingTaskId"] = task_ref_for_type(task.type, task.id);
    } else if (task.type.rfind("judge.submission.", 0) == 0) {
        json["submissionTaskId"] = task_ref_for_type(task.type, task.id);
    }
}

} // namespace task
