#pragma once

#include "db/connection_pool.hpp"
#include "domain/auth/auth_models.hpp"
#include "domain/training/training_models.hpp"
#include "domain/training/training_plan_generation_service.hpp"
#include "domain/task/task_scheduler.hpp"

#include <nlohmann/json.hpp>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace training {

class TrainingBadRequest final : public std::runtime_error {
public:
    explicit TrainingBadRequest(const std::string& message);
};

class TrainingNotFound final : public std::runtime_error {
public:
    explicit TrainingNotFound(const std::string& message);
};

struct TrainingPlanDetail {
    TrainingPlan plan;
    std::vector<TrainingPlanItem> items;
};

class TrainingService final {
public:
    explicit TrainingService(ConnectionPool& pool);

    std::vector<TrainingGoal> listGoals(long long user_id) const;
    TrainingGoal createGoal(long long user_id, const nlohmann::json& body) const;
    std::optional<TrainingGoal> getGoal(long long id, long long user_id) const;
    TrainingGoal updateGoal(long long id, long long user_id, const nlohmann::json& body) const;
    bool deleteGoal(long long id, long long user_id) const;

    long long enqueuePlanGeneration(
        const auth::PublicUser& user,
        const nlohmann::json& body,
        const task::TaskScheduler& scheduler,
        const TrainingPlanGenerationService& generator
    ) const;

    std::vector<TrainingPlan> listPlans(long long user_id) const;
    std::optional<TrainingPlanDetail> getPlanDetail(long long id, long long user_id) const;
    std::optional<std::vector<TrainingPlanItem>> listPlanItems(
        long long id,
        long long user_id
    ) const;

private:
    ConnectionPool& pool_;
};

} // namespace training
