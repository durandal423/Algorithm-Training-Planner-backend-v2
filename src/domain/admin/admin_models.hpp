#pragma once

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace admin {

struct AuditLog {
    long long id{};
    std::optional<long long> actor_user_id;
    std::string action;
    std::string target_type;
    std::optional<long long> target_id;
    std::string metadata_json{"{}"};
    std::string created_at;
};

inline void to_json(nlohmann::json& json, const AuditLog& log) {
    json = {
        {"id", log.id},
        {"actorUserId", log.actor_user_id},
        {"action", log.action},
        {"targetType", log.target_type},
        {"targetId", log.target_id},
        {"metadata", nlohmann::json::parse(log.metadata_json)},
        {"createdAt", log.created_at}
    };
}

} // namespace admin
