#include "db/database.hpp"
#include "domain/task/task_execution_context.hpp"
#include "domain/task/task_handler_registry.hpp"
#include "domain/task/task_queue_repository.hpp"
#include "domain/task/task_worker_runtime.hpp"
#include "logging/logging.hpp"

#include "config.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <string>
#include <thread>

#include <unistd.h>

namespace {

std::atomic_bool shutdown_requested{false};

void request_shutdown(int) {
    shutdown_requested = true;
}

std::string host_name() {
    char buffer[256]{};
    if (gethostname(buffer, sizeof(buffer) - 1) != 0) {
        return "unknown-host";
    }
    return buffer;
}

} // namespace

int main() {
    try {
        initSpdlog();

        std::signal(SIGINT, request_shutdown);
        std::signal(SIGTERM, request_shutdown);

        Database::ensureDatabase();
        Database::ensureSchema();

        ConnectionPool api_pool{
            Database::appConninfo(),
            config::CONNECTION_POOL_MAX_SIZE
        };
        ConnectionPool task_pool{
            Database::appConninfo(),
            config::TASK_CONNECTION_POOL_MAX_SIZE
        };
        ConnectionPool training_pool{
            Database::appConninfo(),
            config::TRAINING_CONNECTION_POOL_MAX_SIZE
        };

        task::TaskQueueRepository repository{task_pool};
        task::TaskExecutionContext execution_context{api_pool, training_pool};
        const auto worker_id = host_name() + "-" + std::to_string(::getpid());
        task::TaskWorkerRuntime runtime{
            repository,
            task::TaskHandlerRegistry::taskHandlerRegistry(),
            execution_context,
            worker_id
        };
        runtime.start();

        spdlog::info("task worker process started");
        while (!shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
        }

        spdlog::info("task worker shutting down");
        runtime.stop();
        spdlog::shutdown();
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("task worker failed: {}", error.what());
        spdlog::shutdown();
        return 1;
    }
}
