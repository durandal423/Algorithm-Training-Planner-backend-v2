#include "domain/task/task_repository.hpp"

#include <pqxx/pqxx>

namespace task {
namespace {

nlohmann::json json_or_empty_object(const pqxx::field_ref field) {
    return nlohmann::json::parse(field.as<std::string>("{}"));
}

std::optional<nlohmann::json> optional_json(const pqxx::field_ref field) {
    const auto text = field.get<std::string>();
    if (!text) {
        return std::nullopt;
    }
    return nlohmann::json::parse(*text);
}

Task task_from_row(const auto& row) {
    return Task{
        .id = row["id"].template as<long long>(),
        .type = row["type"].template as<std::string>(),
        .status = row["status"].template as<std::string>(),
        .owner_user_id = row["owner_user_id"].template get<long long>(),
        .payload = json_or_empty_object(row["payload"]),
        .result = optional_json(row["result"]),
        .error_message = row["error_message"].template get<std::string>(),
        .created_at = row["created_at"].template as<std::string>(),
        .started_at = row["started_at"].template get<std::string>(),
        .finished_at = row["finished_at"].template get<std::string>()
    };
}

constexpr auto kTaskSelect = R"SQL(
    SELECT id,
           type,
           status,
           owner_user_id,
           payload::text AS payload,
           result::text AS result,
           error_message,
           created_at,
           started_at,
           finished_at
    FROM background_tasks
)SQL";

} // namespace

TaskRepository::TaskRepository(ConnectionPool& pool)
    : pool_(pool) {}

long long TaskRepository::createTask(
    const std::string& type,
    std::optional<long long> owner_user_id,
    const nlohmann::json& payload
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO background_tasks(type, owner_user_id, payload)
            VALUES ($1, $2, $3::jsonb)
            RETURNING id
        )SQL",
        pqxx::params{type, owner_user_id, payload.dump()}
    ).one_field().as<long long>();
    tx.commit();
    return id;
}

std::optional<Task> TaskRepository::getTask(
    long long id,
    std::optional<long long> owner_user_id
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kTaskSelect} + R"SQL(
            WHERE id = $1
              AND ($2::bigint IS NULL OR owner_user_id = $2)
        )SQL",
        pqxx::params{id, owner_user_id}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return task_from_row(row.value());
}

void TaskRepository::markRunning(long long id) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        R"SQL(
            UPDATE background_tasks
            SET status = 'running',
                started_at = COALESCE(started_at, now()),
                error_message = NULL
            WHERE id = $1
        )SQL",
        pqxx::params{id}
    ).no_rows();
    tx.commit();
}

void TaskRepository::markSucceeded(
    long long id,
    const nlohmann::json& result
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        R"SQL(
            UPDATE background_tasks
            SET status = 'succeeded',
                result = $2::jsonb,
                error_message = NULL,
                finished_at = now()
            WHERE id = $1
        )SQL",
        pqxx::params{id, result.dump()}
    ).no_rows();
    tx.commit();
}

void TaskRepository::markFailed(
    long long id,
    const std::string& error_message
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    tx.exec(
        R"SQL(
            UPDATE background_tasks
            SET status = 'failed',
                error_message = $2,
                finished_at = now()
            WHERE id = $1
        )SQL",
        pqxx::params{id, error_message}
    ).no_rows();
    tx.commit();
}

} // namespace task
