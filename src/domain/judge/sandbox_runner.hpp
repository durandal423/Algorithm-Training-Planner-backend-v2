#pragma once

#include "domain/judge/judge_models.hpp"

#include <filesystem>
#include <string>

namespace judge {

struct CompileResult {
    bool success{};
    bool timed_out{};
    bool system_error{};
    int exit_code{};
    int time_ms{};
    std::string stdout_sample;
    std::string stderr_sample;
    std::string message;
};

struct RunResult {
    bool timed_out{};
    bool output_limit_exceeded{};
    bool memory_limit_exceeded{};
    bool system_error{};
    int exit_code{};
    int time_ms{};
    int memory_kb{};
    std::string stdout_sample;
    std::string stderr_sample;
    std::string message;
};

class SandboxRunner final {
public:
    explicit SandboxRunner(int pool_slot);

    static void ensureDockerPool();

    CompileResult compile(
        const Submission& submission,
        const JudgeConfig& config,
        const std::filesystem::path& work_dir
    ) const;

    RunResult run(
        const JudgeConfig& config,
        const JudgeTestCase& test_case,
        const std::filesystem::path& work_dir,
        int run_index
    ) const;

private:
    std::string containerName() const;

    int pool_slot_{};
};

} // namespace judge
