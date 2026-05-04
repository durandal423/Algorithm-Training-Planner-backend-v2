#include "domain/task/task_queue_repository.hpp"
#include "domain/task/task_repository.hpp"
#include "test_support/db_test_utils.hpp"

#include <catch2/catch_test_macros.hpp>
#include <pqxx/pqxx>

namespace {

void delete_background_tasks_by_type(const std::string& type) {
    pqxx::connection conn{Database::appConninfo()};
    pqxx::work tx{conn};
    tx.exec(
        "DELETE FROM background_tasks WHERE type = $1",
        pqxx::params{type}
    ).no_rows();
    tx.commit();
}

} // namespace

TEST_CASE("task queue repository claims pending tasks by registered type") {
    if (!test_support::dbTestsEnabled()) {
        SKIP("set RUN_DB_TESTS=1 to run database integration tests");
    }

    test_support::ensureDatabaseReady();
    const auto type = test_support::uniqueName("test.queue.claim.");

    struct Cleanup {
        std::string type;
        ~Cleanup() {
            try {
                delete_background_tasks_by_type(type);
            } catch (...) {
            }
        }
    } cleanup{type};

    ConnectionPool pool{Database::appConninfo(), 4};
    task::TaskRepository tasks{pool};
    task::TaskQueueRepository queue{pool};

    const auto task_id = tasks.createTask(type, std::nullopt, {{"input", "value"}});
    const auto claimed = queue.claimNextPendingTask("worker-a", {type});

    REQUIRE(claimed.has_value());
    REQUIRE(claimed->id == task_id);
    REQUIRE(claimed->type == type);
    REQUIRE(claimed->status == "running");
    REQUIRE(claimed->payload.at("input") == "value");
    REQUIRE_FALSE(queue.claimNextPendingTask("worker-b", {type}).has_value());

    queue.markSucceeded(task_id, {{"ok", true}});
    const auto completed = tasks.getTask(task_id, std::nullopt);
    REQUIRE(completed.has_value());
    REQUIRE(completed->status == "succeeded");
    REQUIRE(completed->result.has_value());
    REQUIRE(completed->result->at("ok") == true);
}
