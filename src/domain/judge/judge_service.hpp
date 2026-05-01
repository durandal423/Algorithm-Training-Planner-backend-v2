#pragma once

#include "domain/judge/judge_repository.hpp"
#include "domain/judge/sandbox_runner.hpp"

#include <string>

namespace judge {

class JudgeService final {
public:
    JudgeService(JudgeRepository& repository, SandboxRunner runner, std::string worker_id);

    bool processNextSubmission();

private:
    JudgeRepository& repository_;
    SandboxRunner runner_;
    std::string worker_id_;
};

} // namespace judge
