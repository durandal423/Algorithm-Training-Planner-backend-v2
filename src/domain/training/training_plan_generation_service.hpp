#pragma once

#include "db/connection_pool.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <string>

namespace training {

struct GenerateTrainingPlanCommand {
    long long user_id{};
    long long goal_id{};
    bool include_hidden{};
    std::optional<std::string> name;
    std::string status{"draft"};
};

class TrainingPlanGenerationService final {
public:
    explicit TrainingPlanGenerationService(ConnectionPool& pool);

    nlohmann::json generatePlan(
        long long task_id,
        const GenerateTrainingPlanCommand& command
    ) const;

private:
    ConnectionPool& pool_;
};

} // namespace training
