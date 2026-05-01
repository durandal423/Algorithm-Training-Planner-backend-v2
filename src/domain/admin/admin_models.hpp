#pragma once

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

} // namespace admin
