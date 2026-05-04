#include "domain/task/task_repository.hpp"

#include <pqxx/pqxx>

namespace task {
namespace {

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

std::optional<Task> TaskRepository::getAuthTaskByToken(
    long long id,
    const std::string& task_token
) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto row = tx.exec(
        std::string{kTaskSelect} + R"SQL(
            WHERE id = $1
              AND owner_user_id IS NULL
              AND type IN ('auth.register', 'auth.login')
              AND payload ->> 'taskToken' = $2
        )SQL",
        pqxx::params{id, task_token}
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
