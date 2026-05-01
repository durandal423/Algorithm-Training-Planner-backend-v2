#include "db/connection_pool.hpp"
#include "domain/judge/judge_repository.hpp"
#include "test_support/db_test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <pqxx/pqxx>
#include <thread>
#include <vector>

namespace {

long long create_problem(const std::string& code) {
    pqxx::connection conn{Database::appConninfo()};
    pqxx::work tx{conn};
    const auto id = tx.exec(
        R"SQL(
            INSERT INTO problems(code, title, difficulty, estimated_minutes, visibility)
            VALUES ($1, $2, 1000, 15, 'public')
            RETURNING id
        )SQL",
        pqxx::params{code, code}
    ).one_field().as<long long>();
    tx.commit();
    return id;
}

} // namespace

TEST_CASE("fetchNextQueuedSubmission does not hand out the same job twice") {
    if (!test_support::dbTestsEnabled()) {
        SUCCEED("Set RUN_DB_TESTS=1 to run PostgreSQL judge queue tests.");
        return;
    }

    test_support::ensureDatabaseReady();
    const auto code = test_support::uniqueName("DB_QUEUE_");
    struct Cleanup {
        std::string code;
        ~Cleanup() {
            try {
                test_support::deleteProblemByCode(code);
            } catch (...) {
            }
        }
    } cleanup{code};

    const auto problem_id = create_problem(code);
    ConnectionPool pool{Database::appConninfo(), 8};
    judge::JudgeRepository repository{pool};

    std::vector<long long> expected_ids;
    for (int i = 0; i < 4; ++i) {
        judge::Submission submission;
        submission.problem_id = problem_id;
        submission.language = "cpp";
        submission.source_code = "int main(){return 0;}";
        expected_ids.push_back(repository.createSubmission(submission));
    }

    std::atomic_bool start{false};
    std::mutex mutex;
    std::vector<long long> picked_ids;
    std::vector<std::thread> workers;
    for (int i = 0; i < 4; ++i) {
        workers.emplace_back([&, i] {
            while (!start.load()) {
                std::this_thread::yield();
            }
            judge::JudgeRepository worker_repository{pool};
            const auto picked = worker_repository.fetchNextQueuedSubmission("test-worker-" + std::to_string(i));
            if (picked) {
                std::lock_guard lock{mutex};
                picked_ids.push_back(picked->id);
            }
        });
    }

    start = true;
    for (auto& worker : workers) {
        worker.join();
    }

    std::sort(picked_ids.begin(), picked_ids.end());
    std::sort(expected_ids.begin(), expected_ids.end());

    REQUIRE(picked_ids == expected_ids);
    REQUIRE_FALSE(repository.fetchNextQueuedSubmission("extra-worker").has_value());
}
