#include "domain/judge/judge_dispatcher.hpp"

#include "config.hpp"
#include "domain/judge/judge_repository.hpp"
#include "domain/judge/judge_service.hpp"
#include "domain/judge/sandbox_runner.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <exception>
#include <string>

#include <unistd.h>

namespace judge {
namespace {

std::string host_name() {
    char buffer[256]{};
    if (gethostname(buffer, sizeof(buffer) - 1) != 0) {
        return "unknown-host";
    }
    return buffer;
}

} // namespace

JudgeDispatcher::JudgeDispatcher(ConnectionPool& pool)
    : pool_(pool) {}

JudgeDispatcher::~JudgeDispatcher() {
    stop();
}

void JudgeDispatcher::start() {
    if (!workers_.empty()) {
        return;
    }

    SandboxRunner::ensureDockerPool();
    stopping_ = false;

    for (int slot = 0; slot < config::JUDGE_DOCKER_POOL_SIZE; ++slot) {
        workers_.emplace_back([this, slot] {
            workerLoop(slot);
        });
    }

    spdlog::info("judge dispatcher started with {} thread(s)", workers_.size());
}

void JudgeDispatcher::stop() {
    stopping_ = true;
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void JudgeDispatcher::workerLoop(int slot) {
    const auto worker_id = host_name() + "-" + std::to_string(::getpid()) + "-" + std::to_string(slot);
    JudgeRepository repository{pool_};
    JudgeService service{repository, SandboxRunner{slot}, worker_id};

    while (!stopping_) {
        try {
            const bool processed = service.processNextSubmission();
            if (!processed) {
                std::this_thread::sleep_for(std::chrono::milliseconds{100});
            }
        } catch (const std::exception& error) {
            spdlog::error("judge worker loop error (slot={}): {}", slot, error.what());
            std::this_thread::sleep_for(std::chrono::seconds{1});
        }
    }
}

} // namespace judge
