#pragma once

#include "db/connection_pool.hpp"
#include "domain/admin/admin_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace admin {

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
