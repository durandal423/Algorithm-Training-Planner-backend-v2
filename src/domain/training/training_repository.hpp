#pragma once

#include "db/connection_pool.hpp"
#include "domain/training/training_models.hpp"

#include <optional>
#include <string>
#include <vector>

namespace training {

class TrainingRepository final {
public:
    explicit TrainingRepository(ConnectionPool& pool);

    long long createGoal(const TrainingGoal& goal) const;
    std::vector<TrainingGoal> listGoals(long long user_id) const;

    long long createPlan(const TrainingPlan& plan, const std::vector<TrainingPlanItem>& items) const;
    std::vector<TrainingPlan> listPlans(long long user_id) const;
    std::vector<TrainingPlanItem> listPlanItems(long long plan_id) const;
    bool updatePlanItemFromSubmission(long long submission_id, const std::string& verdict) const;

    long long createRecord(const TrainingRecord& record) const;

private:
    ConnectionPool& pool_;

    long long upsertTag(pqxx::transaction_base& tx, const std::string& name) const;
    void replaceGoalTags(
        pqxx::transaction_base& tx,
        long long goal_id,
        const std::vector<std::string>& names
    ) const;
};

} // namespace training
