#pragma once

#include "db/connection_pool.hpp"
#include "domain/admin/admin_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace admin {

template <typename Row>
AuditLog audit_log_from_row(const Row& row) {
    AuditLog log;
    log.id = row["id"].template as<long long>();
    log.actor_user_id = row["actor_user_id"].template get<long long>();
    log.action = row["action"].template as<std::string>();
    log.target_type = row["target_type"].template as<std::string>();
    log.target_id = row["target_id"].template get<long long>();
    log.metadata_json = row["metadata"].template as<std::string>("");
    log.created_at = row["created_at"].template as<std::string>("");
    return log;
}

class AdminRepository final {
public:
    explicit AdminRepository(ConnectionPool& pool);

    long long insertAuditLog(
        std::optional<long long> actor_user_id,
        const std::string& action,
        const std::string& target_type,
        std::optional<long long> target_id,
        const std::string& metadata_json
    ) const;

    std::vector<AuditLog> listAuditLogs(int limit) const;

private:
    ConnectionPool& pool_;
};

} // namespace admin
