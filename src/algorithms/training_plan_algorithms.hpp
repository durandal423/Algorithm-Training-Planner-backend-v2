#pragma once

#include "domain/training/training_models.hpp"

#include <vector>

namespace training {

StableDifficultyWindow selectStableDifficultyWindow(
    const std::vector<TrainingProblemCandidate>& candidates,
    const TrainingGoal& goal
);

OptimizedTrainingPlan optimizeTrainingPlanByDP(
    const StableDifficultyWindow& window,
    const TrainingGoal& goal
);

int scoreProblemForTrainingGoal(
    const TrainingProblemCandidate& candidate,
    const TrainingGoal& goal,
    int window_center_difficulty
);

} // namespace training
