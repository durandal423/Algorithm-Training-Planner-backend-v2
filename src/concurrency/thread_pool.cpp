#include "concurrency/thread_pool.hpp"

#include <spdlog/spdlog.h>

#include <stdexcept>
#include <utility>

namespace concurrency {

ThreadPool::ThreadPool(
    std::string name,
    std::size_t worker_count,
    std::size_t queue_capacity
)
    : name_(std::move(name)),
      queue_capacity_(queue_capacity) {
    if (worker_count == 0) {
        throw std::invalid_argument{"thread pool worker_count must be greater than 0"};
    }
    if (queue_capacity_ == 0) {
        throw std::invalid_argument{"thread pool queue_capacity must be greater than 0"};
    }

    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this, i] {
            workerLoop(i);
        });
    }

    spdlog::info(
        "thread pool '{}' started with {} worker(s), queue_capacity={}",
        name_,
        worker_count,
        queue_capacity_
    );
}

ThreadPool::~ThreadPool() {
    stop();
}

bool ThreadPool::trySubmit(std::function<void()> task) {
    {
        std::lock_guard lock{mutex_};
        if (stopping_ || tasks_.size() >= queue_capacity_) {
            return false;
        }
        tasks_.push_back(std::move(task));
    }

    available_cv_.notify_one();
    return true;
}

void ThreadPool::stop() {
    {
        std::lock_guard lock{mutex_};
        if (stopping_) {
            return;
        }
        stopping_ = true;
    }

    available_cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPool::workerLoop(std::size_t worker_index) {
    while (true) {
        std::function<void()> task;

        {
            std::unique_lock lock{mutex_};
            available_cv_.wait(lock, [&] {
                return stopping_ || !tasks_.empty();
            });

            if (stopping_ && tasks_.empty()) {
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
        }

        try {
            task();
        } catch (const std::exception& error) {
            spdlog::error(
                "thread pool '{}' worker {} task escaped with exception: {}",
                name_,
                worker_index,
                error.what()
            );
        } catch (...) {
            spdlog::error(
                "thread pool '{}' worker {} task escaped with unknown exception",
                name_,
                worker_index
            );
        }
    }
}

} // namespace concurrency
