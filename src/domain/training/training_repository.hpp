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
    std::optional<TrainingGoal> getGoal(long long id, long long user_id) const;
    std::vector<TrainingGoal> listGoals(long long user_id) const;
    bool updateGoal(long long id, const TrainingGoal& goal) const;
    bool deleteGoal(long long id, long long user_id) const;

    long long createPlan(const TrainingPlan& plan, const std::vector<TrainingPlanItem>& items) const;
    std::optional<TrainingPlan> getPlan(long long id, long long user_id) const;
    std::vector<TrainingPlan> listPlans(long long user_id) const;
    std::vector<TrainingPlanItem> listPlanItems(long long plan_id) const;
    std::vector<TrainingProblemCandidate> listProblemCandidates(
        const TrainingGoal& goal,
        bool include_hidden
    ) const;
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
    std::vector<std::string> loadGoalTags(pqxx::transaction_base& tx, long long goal_id) const;
};

} // namespace training
