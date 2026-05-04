#include "domain/task/task_queue_repository.hpp"

#include "domain/task/task_repository.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/pqxx>

namespace task {

TaskQueueRepository::TaskQueueRepository(ConnectionPool& pool)
    : pool_(pool) {}

std::optional<Task> TaskQueueRepository::claimNextPendingTask(
    const std::string& worker_id,
    const std::vector<std::string>& types
) const {
    if (types.empty()) {
        return std::nullopt;
    }

    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto type_filter = nlohmann::json(types).dump();
    const auto row = tx.exec(
        R"SQL(
            WITH picked AS (
                SELECT id
                FROM background_tasks
                WHERE status = 'pending'
                  AND run_after <= now()
                  AND type IN (
                      SELECT jsonb_array_elements_text($2::jsonb)
                  )
                ORDER BY run_after, created_at, id
                FOR UPDATE SKIP LOCKED
                LIMIT 1
            )
            UPDATE background_tasks task
            SET status = 'running',
                locked_by = $1,
                locked_at = now(),
                attempt_count = attempt_count + 1,
                started_at = COALESCE(started_at, now()),
                finished_at = NULL,
                error_message = NULL
            FROM picked
            WHERE task.id = picked.id
            RETURNING task.id,
                      task.type,
                      task.status,
                      task.owner_user_id,
                      task.payload::text AS payload,
                      task.result::text AS result,
                      task.error_message,
                      task.created_at,
                      task.started_at,
                      task.finished_at
        )SQL",
        pqxx::params{worker_id, type_filter}
    ).opt_row();
    tx.commit();
    if (!row) {
        return std::nullopt;
    }
    return task_from_row(row.value());
}

void TaskQueueRepository::markSucceeded(
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
                locked_by = NULL,
                locked_at = NULL,
                finished_at = now()
            WHERE id = $1
        )SQL",
        pqxx::params{id, result.dump()}
    ).no_rows();
    tx.commit();
}

void TaskQueueRepository::markFailed(
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
                locked_by = NULL,
                locked_at = NULL,
                finished_at = now()
            WHERE id = $1
        )SQL",
        pqxx::params{id, error_message}
    ).no_rows();
    tx.commit();
}

int TaskQueueRepository::recoverStaleRunningTasks(int stale_after_seconds) const {
    auto lease = pool_.acquire();
    pqxx::work tx{lease.connection()};
    const auto rows = tx.exec(
        R"SQL(
            UPDATE background_tasks
            SET status = CASE
                    WHEN attempt_count >= max_attempts THEN 'failed'
                    ELSE 'pending'
                END,
                error_message = CASE
                    WHEN attempt_count >= max_attempts THEN 'background task worker lease expired'
                    ELSE NULL
                END,
                locked_by = NULL,
                locked_at = NULL,
                started_at = CASE
                    WHEN attempt_count >= max_attempts THEN started_at
                    ELSE NULL
                END,
                finished_at = CASE
                    WHEN attempt_count >= max_attempts THEN now()
                    ELSE NULL
                END
            WHERE status = 'running'
              AND locked_at IS NOT NULL
              AND locked_at < now() - ($1::integer * interval '1 second')
            RETURNING id
        )SQL",
        pqxx::params{stale_after_seconds}
    );
    tx.commit();
    return static_cast<int>(rows.size());
}

} // namespace task
