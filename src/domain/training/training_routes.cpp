#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "algorithms/training_plan_algorithms.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/training/training_repository.hpp"

#include <exception>
#include <optional>
#include <string>
#include <vector>

namespace {

training::TrainingGoal goal_from_json(
    const nlohmann::json& body,
    long long user_id
) {
    training::TrainingGoal goal;
    goal.user_id = user_id;
    goal.name = body.at("name").get<std::string>();
    goal.target_count = body.value("targetCount", 10);
    goal.time_budget_minutes = body.value("timeBudgetMinutes", 180);
    goal.difficulty_min = api::optional_int(body, "difficultyMin");
    goal.difficulty_max = api::optional_int(body, "difficultyMax");
    goal.prefer_wrong_problems = body.value("preferWrongProblems", true);
    goal.prefer_weak_tags = body.value("preferWeakTags", true);
    goal.difficulty_weight = body.value("difficultyWeight", 10);
    goal.tag_coverage_weight = body.value("tagCoverageWeight", 20);
    goal.wrong_problem_weight = body.value("wrongProblemWeight", 15);
    goal.weak_tag_weight = body.value("weakTagWeight", 15);
    goal.estimated_time_weight = body.value("estimatedTimeWeight", 1);
    goal.tag_names = api::string_array(body, "tags");
    return goal;
}

std::vector<training::TrainingPlanItem> select_items(
    const training::OptimizedTrainingPlan& optimized
) {
    std::vector<training::TrainingPlanItem> items;
    for (const auto& selected : optimized.items) {
        training::TrainingPlanItem item;
        item.problem_id = selected.problem.id;
        item.order_index = static_cast<int>(items.size()) + 1;
        item.estimated_minutes = selected.problem.estimated_minutes;
        item.score = selected.score;
        item.selected_reason = selected.selected_reason;
        item.status = "not_started";
        items.push_back(std::move(item));
    }
    return items;
}

RouteRegistration trainingRoutesRegistration([](App& app, ConnectionPool& connection_pool) {
    CROW_ROUTE(app, "/api/training/goals")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingRepository repository{connection_pool};
            if (req.method == crow::HTTPMethod::Get) {
                return api::json_response(200, {{"items", repository.listGoals(user.id)}});
            }

            const auto body = nlohmann::json::parse(req.body);
            const auto id = repository.createGoal(goal_from_json(body, user.id));
            const auto goal = repository.getGoal(id, user.id);
            return api::json_response(201, {{"goal", *goal}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/training/goals/<int>")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Put, crow::HTTPMethod::Delete)
    ([&connection_pool](const crow::request& req, int id) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingRepository repository{connection_pool};
            if (req.method == crow::HTTPMethod::Get) {
                const auto goal = repository.getGoal(id, user.id);
                return goal
                    ? api::json_response(200, {{"goal", *goal}})
                    : api::json_response(404, {{"error", "training goal not found"}});
            }
            if (req.method == crow::HTTPMethod::Delete) {
                return repository.deleteGoal(id, user.id)
                    ? api::json_response(200, {{"deleted", true}})
                    : api::json_response(404, {{"error", "training goal not found"}});
            }

            const auto body = nlohmann::json::parse(req.body);
            const bool updated = repository.updateGoal(id, goal_from_json(body, user.id));
            if (!updated) {
                return api::json_response(404, {{"error", "training goal not found"}});
            }
            const auto goal = repository.getGoal(id, user.id);
            return api::json_response(200, {{"goal", *goal}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/training/plans/generate")
    .methods(crow::HTTPMethod::Post)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            const auto body = nlohmann::json::parse(req.body);
            const auto goal_id = api::optional_long(body, "goalId");
            if (!goal_id) {
                return api::json_response(400, {{"error", "goalId is required"}});
            }

            training::TrainingRepository repository{connection_pool};
            const auto goal = repository.getGoal(*goal_id, user.id);
            if (!goal) {
                return api::json_response(404, {{"error", "training goal not found"}});
            }

            const auto candidates = repository.listProblemCandidates(*goal, user.role == "admin");
            const auto window = training::selectStableDifficultyWindow(candidates, *goal);
            const auto optimized = training::optimizeTrainingPlanByDP(window, *goal);
            auto items = select_items(optimized);

            training::TrainingPlan plan;
            plan.user_id = user.id;
            plan.goal_id = goal->id;
            plan.name = body.value("name", goal->name + " plan");
            plan.status = body.value("status", std::string{"draft"});
            plan.candidate_count = static_cast<int>(candidates.size());
            plan.selected_count = static_cast<int>(items.size());
            plan.total_estimated_minutes = optimized.total_estimated_minutes;
            plan.algorithm_summary =
                "selectStableDifficultyWindow: two-pointer stable difficulty window [" +
                std::to_string(window.left_difficulty) + ", " +
                std::to_string(window.right_difficulty) + "], span=" +
                std::to_string(window.difficulty_span) + ", windowCandidates=" +
                std::to_string(window.candidates.size()) +
                "; optimizeTrainingPlanByDP: 0/1 knapsack by time budget and target count, totalScore=" +
                std::to_string(optimized.total_score);

            const auto plan_id = repository.createPlan(plan, items);
            const auto saved_plan = repository.getPlan(plan_id, user.id);
            const auto saved_items = repository.listPlanItems(plan_id);
            return api::json_response(201, {
                {"plan", *saved_plan},
                {"items", saved_items}
            });
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/training/plans")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingRepository repository{connection_pool};
            return api::json_response(200, {{"items", repository.listPlans(user.id)}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/training/plans/<int>")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req, int id) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingRepository repository{connection_pool};
            const auto plan = repository.getPlan(id, user.id);
            if (!plan) {
                return api::json_response(404, {{"error", "training plan not found"}});
            }
            return api::json_response(200, {
                {"plan", *plan},
                {"items", repository.listPlanItems(id)}
            });
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });

    CROW_ROUTE(app, "/api/training/plans/<int>/items")
    .methods(crow::HTTPMethod::Get)
    ([&connection_pool](const crow::request& req, int id) -> crow::response {
        try {
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingRepository repository{connection_pool};
            const auto plan = repository.getPlan(id, user.id);
            if (!plan) {
                return api::json_response(404, {{"error", "training plan not found"}});
            }
            return api::json_response(200, {{"items", repository.listPlanItems(id)}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return api::json_response(400, {{"error", error.what()}});
        }
    });
});

} // namespace
