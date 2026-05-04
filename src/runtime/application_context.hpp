#pragma once

#include "db/connection_pool.hpp"
#include "domain/task/task_repository.hpp"
#include "domain/task/task_submission_service.hpp"

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
    const task::TaskSubmissionService& taskSubmissionService() const;

    void stop();

private:
    ConnectionPool api_pool_;
    ConnectionPool task_pool_;
    task::TaskRepository task_repository_;
    task::TaskSubmissionService task_submission_service_;
};

} // namespace runtime
