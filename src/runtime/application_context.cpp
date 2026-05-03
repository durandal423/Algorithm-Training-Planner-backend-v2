#include "runtime/application_context.hpp"

#include "config.hpp"

namespace runtime {

ApplicationContext::ApplicationContext(const std::string& app_conninfo)
    : api_pool_{app_conninfo},
      task_pool_{app_conninfo, config::TASK_CONNECTION_POOL_MAX_SIZE},
      training_pool_{app_conninfo, config::TRAINING_CONNECTION_POOL_MAX_SIZE},
      background_pool_{
          "background-tasks",
          config::BACKGROUND_TASK_THREAD_COUNT,
          config::BACKGROUND_TASK_QUEUE_CAPACITY
      },
      task_repository_{task_pool_},
      task_scheduler_{task_repository_, background_pool_},
      training_plan_generator_{training_pool_} {}

ApplicationContext::~ApplicationContext() {
    stop();
}

ConnectionPool& ApplicationContext::apiPool() {
    return api_pool_;
}

task::TaskRepository& ApplicationContext::taskRepository() {
    return task_repository_;
}

const task::TaskScheduler& ApplicationContext::taskScheduler() const {
    return task_scheduler_;
}

const training::TrainingPlanGenerationService& ApplicationContext::trainingPlanGenerator() const {
    return training_plan_generator_;
}

void ApplicationContext::stop() {
    background_pool_.stop();
}

} // namespace runtime
