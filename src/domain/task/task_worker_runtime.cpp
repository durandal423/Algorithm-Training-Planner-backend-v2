#include "domain/task/task_worker_runtime.hpp"

#include "config.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <map>
#include <utility>

namespace task {

TaskWorkerRuntime::TaskWorkerRuntime(
    TaskQueueRepository& repository,
    TaskHandlerRegistry& registry,
    TaskExecutionContext& context,
    std::string worker_id
)
    : repository_(repository),
      registry_(registry),
      context_(context),
      worker_id_(std::move(worker_id)) {}

TaskWorkerRuntime::~TaskWorkerRuntime() {
    stop();
}

void TaskWorkerRuntime::start() {
    if (!workers_.empty()) {
        return;
    }

    stopping_ = false;
    const auto pools = poolSpecs();
    for (const auto& pool : pools) {
        for (std::size_t index = 0; index < pool.worker_count; ++index) {
            workers_.emplace_back([this, pool, index] {
                workerLoop(pool, index);
            });
        }
    }

    recovery_worker_ = std::thread{[this] {
        recoveryLoop();
    }};

    spdlog::info(
        "task worker '{}' started with {} pool(s) and {} thread(s)",
        worker_id_,
        pools.size(),
        workers_.size()
    );
}

void TaskWorkerRuntime::stop() {
    if (stopping_.exchange(true)) {
        return;
    }

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();

    if (recovery_worker_.joinable()) {
        recovery_worker_.join();
    }
}

std::vector<TaskWorkerRuntime::PoolSpec> TaskWorkerRuntime::poolSpecs() const {
    std::map<std::string, PoolSpec> by_name;

    for (const auto& handler : registry_.handlers()) {
        auto [iter, inserted] = by_name.try_emplace(
            handler.pool_name,
            PoolSpec{
                .name = handler.pool_name,
                .types = {},
                .worker_count = handler.worker_count
            }
        );
        iter->second.types.push_back(handler.type);
        iter->second.worker_count = std::max(iter->second.worker_count, handler.worker_count);
    }

    std::vector<PoolSpec> pools;
    pools.reserve(by_name.size());
    for (auto& [_, pool] : by_name) {
        pools.push_back(std::move(pool));
    }
    return pools;
}

void TaskWorkerRuntime::workerLoop(const PoolSpec& pool, std::size_t worker_index) {
    const auto local_worker_id = worker_id_ + ":" + pool.name + ":" + std::to_string(worker_index);

    while (!stopping_) {
        try {
            const auto task = repository_.claimNextPendingTask(local_worker_id, pool.types);
            if (!task) {
                std::this_thread::sleep_for(
                    std::chrono::milliseconds{config::TASK_WORKER_EMPTY_SLEEP_MS}
                );
                continue;
            }

            const auto* handler = registry_.find(task->type);
            if (handler == nullptr) {
                repository_.markFailed(task->id, "no task handler registered for type: " + task->type);
                continue;
            }

            spdlog::info(
                "background task picked up (task_id={}, type={}, worker={})",
                task->id,
                task->type,
                local_worker_id
            );

            try {
                const auto result = handler->handler(context_, *task);
                repository_.markSucceeded(task->id, result);
                spdlog::info(
                    "background task succeeded (task_id={}, type={}, worker={})",
                    task->id,
                    task->type,
                    local_worker_id
                );
            } catch (const std::exception& error) {
                repository_.markFailed(task->id, error.what());
                spdlog::error(
                    "background task failed (task_id={}, type={}, worker={}): {}",
                    task->id,
                    task->type,
                    local_worker_id,
                    error.what()
                );
            } catch (...) {
                repository_.markFailed(task->id, "unknown background task error");
                spdlog::error(
                    "background task failed (task_id={}, type={}, worker={}): unknown error",
                    task->id,
                    task->type,
                    local_worker_id
                );
            }
        } catch (const std::exception& error) {
            spdlog::error(
                "task worker loop error (pool={}, worker={}): {}",
                pool.name,
                local_worker_id,
                error.what()
            );
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
}

void TaskWorkerRuntime::recoveryLoop() {
    while (!stopping_) {
        try {
            const auto recovered = repository_.recoverStaleRunningTasks(
                config::TASK_WORKER_STALE_SECONDS
            );
            if (recovered > 0) {
                spdlog::warn("recovered {} stale background task(s)", recovered);
            }
        } catch (const std::exception& error) {
            spdlog::error("background task stale recovery failed: {}", error.what());
        }

        for (int elapsed = 0;
             elapsed < config::TASK_WORKER_STALE_RECOVERY_INTERVAL_SECONDS && !stopping_;
             ++elapsed) {
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
}

} // namespace task
