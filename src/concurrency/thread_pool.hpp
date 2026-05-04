#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace concurrency {

class ThreadPool final {
public:
    ThreadPool(std::string name, std::size_t worker_count, std::size_t queue_capacity);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    bool trySubmit(std::function<void()> task);
    bool trySubmit(std::string lane, std::function<void()> task);
    void stop();

private:
    void workerLoop(std::size_t worker_index);

    std::string name_;
    std::size_t queue_capacity_{};
    std::size_t queued_count_{0};
    std::size_t next_lane_index_{0};
    bool stopping_{false};

    std::mutex mutex_;
    std::condition_variable available_cv_;
    std::vector<std::string> lanes_;
    std::unordered_map<std::string, std::deque<std::function<void()>>> tasks_by_lane_;
    std::vector<std::thread> workers_;
};

} // namespace concurrency
