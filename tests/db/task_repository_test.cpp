#include "domain/task/task_repository.hpp"

#include "test_support/db_test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

TEST_CASE("background task repository stores task lifecycle") {
    if (!test_support::dbTestsEnabled()) {
        SKIP("set RUN_DB_TESTS=1 to run database integration tests");
    }

    Database::ensureDatabase();
    Database::ensureSchema();

    ConnectionPool pool{Database::appConninfo(), 4};
    task::TaskRepository repository{pool};

    const auto id = repository.createTask(
        "test.background",
        std::nullopt,
        {{"input", "value"}}
    );

    auto task = repository.getTask(id, std::nullopt);
    REQUIRE(task.has_value());
    REQUIRE(task->status == "pending");
    REQUIRE(task->payload.at("input") == "value");

    repository.markRunning(id);
    task = repository.getTask(id, std::nullopt);
    REQUIRE(task.has_value());
    REQUIRE(task->status == "running");
    REQUIRE(task->started_at.has_value());

    repository.markSucceeded(id, {{"ok", true}});
    task = repository.getTask(id, std::nullopt);
    REQUIRE(task.has_value());
    REQUIRE(task->status == "succeeded");
    REQUIRE(task->result.has_value());
    REQUIRE(task->result->at("ok") == true);
    REQUIRE(task->finished_at.has_value());
}
