#pragma once

#include "domain/task/task_execution_context.hpp"
#include "domain/task/task_models.hpp"

#include <nlohmann/json.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace task {

using TaskHandler = std::function<nlohmann::json(TaskExecutionContext&, const Task&)>;

struct TaskHandlerSpec {
    std::string type;
    std::string pool_name{"default"};
    std::size_t worker_count{1};
    TaskHandler handler;
};

class TaskHandlerRegistry final {
public:
    void add(TaskHandlerSpec spec);

    const TaskHandlerSpec* find(const std::string& type) const;
    const std::vector<TaskHandlerSpec>& handlers() const;

    static TaskHandlerRegistry& taskHandlerRegistry();

private:
    std::vector<TaskHandlerSpec> handlers_;
};

class TaskHandlerRegistration final {
public:
    explicit TaskHandlerRegistration(TaskHandlerSpec spec);
};

} // namespace task
