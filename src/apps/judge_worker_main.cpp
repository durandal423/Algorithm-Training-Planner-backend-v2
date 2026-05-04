#include "db/database.hpp"
#include "domain/judge/judge_dispatcher.hpp"
#include "logging/logging.hpp"

#include "config.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <exception>
#include <thread>

namespace {

std::atomic_bool shutdown_requested{false};

void request_shutdown(int) {
    shutdown_requested = true;
}

} // namespace

int main() {
    try {
        initSpdlog();

        std::signal(SIGINT, request_shutdown);
        std::signal(SIGTERM, request_shutdown);

        Database::ensureDatabase();
        Database::ensureSchema();

        ConnectionPool judge_pool{
            Database::appConninfo(),
            config::JUDGE_CONNECTION_POOL_MAX_SIZE
        };
        judge::JudgeDispatcher dispatcher{judge_pool};
        dispatcher.start();

        spdlog::info("judge worker process started");
        while (!shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds{200});
        }

        spdlog::info("judge worker shutting down");
        dispatcher.stop();
        spdlog::shutdown();
        return 0;
    } catch (const std::exception& error) {
        spdlog::error("judge worker failed: {}", error.what());
        spdlog::shutdown();
        return 1;
    }
}
