#include "runtime/application_context.hpp"

#include "config.hpp"

namespace runtime {

ApplicationContext::ApplicationContext(const std::string& app_conninfo)
    : api_pool_{app_conninfo},
      task_pool_{app_conninfo, config::TASK_CONNECTION_POOL_MAX_SIZE},
      task_repository_{task_pool_},
      task_submission_service_{task_repository_} {}

ApplicationContext::~ApplicationContext() {
    stop();
}

ConnectionPool& ApplicationContext::apiPool() {
    return api_pool_;
}

task::TaskRepository& ApplicationContext::taskRepository() {
    return task_repository_;
}

const task::TaskSubmissionService& ApplicationContext::taskSubmissionService() const {
    return task_submission_service_;
}

void ApplicationContext::stop() {
}

} // namespace runtime
