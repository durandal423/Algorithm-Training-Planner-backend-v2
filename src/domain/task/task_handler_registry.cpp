#include "domain/task/task_handler_registry.hpp"

#include <stdexcept>
#include <utility>

namespace task {

void TaskHandlerRegistry::add(TaskHandlerSpec spec) {
    if (spec.type.empty()) {
        throw std::invalid_argument{"task handler type must not be empty"};
    }
    if (spec.pool_name.empty()) {
        spec.pool_name = "default";
    }
    if (spec.worker_count == 0) {
        throw std::invalid_argument{"task handler worker_count must be greater than 0"};
    }
    if (!spec.handler) {
        throw std::invalid_argument{"task handler must not be empty"};
    }
    if (find(spec.type) != nullptr) {
        throw std::invalid_argument{"duplicate task handler registration: " + spec.type};
    }
    handlers_.push_back(std::move(spec));
}

const TaskHandlerSpec* TaskHandlerRegistry::find(const std::string& type) const {
    for (const auto& handler : handlers_) {
        if (handler.type == type) {
            return &handler;
        }
    }
    return nullptr;
}

const std::vector<TaskHandlerSpec>& TaskHandlerRegistry::handlers() const {
    return handlers_;
}

TaskHandlerRegistry& TaskHandlerRegistry::taskHandlerRegistry() {
    static TaskHandlerRegistry registry;
    return registry;
}

TaskHandlerRegistration::TaskHandlerRegistration(TaskHandlerSpec spec) {
    TaskHandlerRegistry::taskHandlerRegistry().add(std::move(spec));
}

} // namespace task
