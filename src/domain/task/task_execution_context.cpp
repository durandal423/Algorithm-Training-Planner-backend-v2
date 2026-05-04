#include "domain/task/task_execution_context.hpp"

namespace task {

TaskExecutionContext::TaskExecutionContext(
    ConnectionPool& api_pool,
    ConnectionPool& training_pool
)
    : api_pool_(api_pool),
      training_pool_(training_pool) {}

ConnectionPool& TaskExecutionContext::apiPool() const {
    return api_pool_;
}

ConnectionPool& TaskExecutionContext::trainingPool() const {
    return training_pool_;
}

} // namespace task
