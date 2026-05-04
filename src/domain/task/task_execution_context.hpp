#pragma once

#include "db/connection_pool.hpp"

namespace task {

class TaskExecutionContext final {
public:
    TaskExecutionContext(ConnectionPool& api_pool, ConnectionPool& training_pool);

    ConnectionPool& apiPool() const;
    ConnectionPool& trainingPool() const;

private:
    ConnectionPool& api_pool_;
    ConnectionPool& training_pool_;
};

} // namespace task
