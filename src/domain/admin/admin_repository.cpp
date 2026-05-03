#include "domain/admin/admin_repository.hpp"

#include <pqxx/pqxx>

#include <algorithm>

namespace admin {
namespace {

std::optional<long long> optional_long(const pqxx::field_ref field) {
    if (field.is_null()) {
        return std::nullopt;
    }
    return field.as<long long>();
}

std::string string_or_empty(const pqxx::field_ref field) {
    if (field.is_null()) {
        return {};
    }
    return field.as<std::string>();
}

AuditLog audit_log_from_row(const pqxx::row& row) {
    AuditLog log;
    log.id = row["id"].as<long long>();
    log.actor_user_id = optional_long(row["actor_user_id"]);
    log.action = row["action"].as<std::string>();
    log.target_type = row["target_type"].as<std::string>();
    log.target_id = optional_long(row["target_id"]);
    log.metadata_json = string_or_empty(row["metadata"]);
    log.created_at = string_or_empty(row["created_at"]);
    return log;
}

AuditLog audit_log_from_row(const pqxx::row_ref& row) {
    return audit_log_from_row(pqxx::row{row});
}

} // namespace

AdminRepository::AdminRepository(ConnectionPool& pool)
    : pool_(pool) {}

long long AdminRepository::insertAuditLog(
    std::optional<long long> actor_user_id,
    const std::string& action,
    const std::string& target_type,
    std::optional<long long> target_id,
    const std::string& metadata_json
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO audit_logs(actor_user_id, action, target_type, target_id, metadata)
            VALUES ($1, $2, $3, $4, $5::jsonb)
            RETURNING id
        )SQL",
        pqxx::params{actor_user_id, action, target_type, target_id, metadata_json}
    ).one_field().as<long long>();
    tx.commit();
    return id;
}

std::vector<AuditLog> AdminRepository::listAuditLogs(int limit) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    std::vector<AuditLog> logs;
    const auto rows = tx.exec(
        R"SQL(
            SELECT id, actor_user_id, action, target_type, target_id,
                   metadata::text AS metadata, created_at
            FROM audit_logs
            ORDER BY created_at DESC, id DESC
            LIMIT $1
        )SQL",
        pqxx::params{std::clamp(limit, 1, 200)}
    );
    for (const auto& row : rows) {
        logs.push_back(audit_log_from_row(row));
    }
    tx.commit();
    return logs;
}

} // namespace admin
