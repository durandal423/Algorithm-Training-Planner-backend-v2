#pragma once

#include "db/connection_pool.hpp"

#include <atomic>
#include <thread>
#include <vector>

namespace judge {

class JudgeDispatcher final {
public:
    explicit JudgeDispatcher(ConnectionPool& pool);
    ~JudgeDispatcher();

    JudgeDispatcher(const JudgeDispatcher&) = delete;
    JudgeDispatcher& operator=(const JudgeDispatcher&) = delete;

    void start();
    void stop();

private:
    void workerLoop(int slot);

    ConnectionPool& pool_;
    std::atomic_bool stopping_{false};
    std::vector<std::thread> workers_;
};

} // namespace judge
