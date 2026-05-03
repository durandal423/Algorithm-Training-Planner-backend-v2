#pragma once

#include "db/connection_pool.hpp"
#include "domain/training/training_plan_generation_service.hpp"
#include "domain/task/task_repository.hpp"
#include "domain/task/task_scheduler.hpp"
#include "concurrency/thread_pool.hpp"

#include <string>

namespace runtime {

class ApplicationContext final {
public:
    explicit ApplicationContext(const std::string& app_conninfo);
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;

    ConnectionPool& apiPool();
    task::TaskRepository& taskRepository();
    const task::TaskScheduler& taskScheduler() const;
    const training::TrainingPlanGenerationService& trainingPlanGenerator() const;

    void stop();

private:
    ConnectionPool api_pool_;
    ConnectionPool task_pool_;
    ConnectionPool training_pool_;
    concurrency::ThreadPool background_pool_;
    task::TaskRepository task_repository_;
    task::TaskScheduler task_scheduler_;
    training::TrainingPlanGenerationService training_plan_generator_;
};

} // namespace runtime
