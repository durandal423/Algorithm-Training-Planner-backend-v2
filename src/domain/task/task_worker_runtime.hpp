#pragma once

#include "domain/task/task_execution_context.hpp"
#include "domain/task/task_handler_registry.hpp"
#include "domain/task/task_queue_repository.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <vector>

namespace task {

class TaskWorkerRuntime final {
public:
    TaskWorkerRuntime(
        TaskQueueRepository& repository,
        TaskHandlerRegistry& registry,
        TaskExecutionContext& context,
        std::string worker_id
    );
    ~TaskWorkerRuntime();

    TaskWorkerRuntime(const TaskWorkerRuntime&) = delete;
    TaskWorkerRuntime& operator=(const TaskWorkerRuntime&) = delete;

    void start();
    void stop();

private:
    struct PoolSpec {
        std::string name;
        std::vector<std::string> types;
        std::size_t worker_count{};
    };

    std::vector<PoolSpec> poolSpecs() const;
    void workerLoop(const PoolSpec& pool, std::size_t worker_index);
    void recoveryLoop();

    TaskQueueRepository& repository_;
    TaskHandlerRegistry& registry_;
    TaskExecutionContext& context_;
    std::string worker_id_;
    std::atomic_bool stopping_{false};
    std::vector<std::thread> workers_;
    std::thread recovery_worker_;
};

} // namespace task
