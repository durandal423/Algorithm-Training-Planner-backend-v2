#include "api/route_registry.hpp"
#include "api/http_json.hpp"
#include "domain/auth/auth_http.hpp"
#include "domain/task/task_models.hpp"
#include "domain/training/training_service.hpp"

#include <exception>

namespace {

crow::response training_error_response(const std::exception& error) {
    return api::json_response(400, {{"error", error.what()}});
}

RouteRegistration trainingRoutesRegistration([](App& app, runtime::ApplicationContext& context) {
    CROW_ROUTE(app, "/api/training/goals")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingService service{connection_pool};
            if (req.method == crow::HTTPMethod::Get) {
                return api::json_response(200, {{"items", service.listGoals(user.id)}});
            }

            const auto body = nlohmann::json::parse(req.body);
            return api::json_response(201, {{"goal", service.createGoal(user.id, body)}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return training_error_response(error);
        }
    });

    CROW_ROUTE(app, "/api/training/goals/<int>")
    .methods(crow::HTTPMethod::Get, crow::HTTPMethod::Put, crow::HTTPMethod::Delete)
    ([&context](const crow::request& req, int id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingService service{connection_pool};
            if (req.method == crow::HTTPMethod::Get) {
                const auto goal = service.getGoal(id, user.id);
                return goal
                    ? api::json_response(200, {{"goal", *goal}})
                    : api::json_response(404, {{"error", "training goal not found"}});
            }
            if (req.method == crow::HTTPMethod::Delete) {
                return service.deleteGoal(id, user.id)
                    ? api::json_response(200, {{"deleted", true}})
                    : api::json_response(404, {{"error", "training goal not found"}});
            }

            const auto body = nlohmann::json::parse(req.body);
            return api::json_response(200, {{"goal", service.updateGoal(id, user.id, body)}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const training::TrainingNotFound& error) {
            return api::json_response(404, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return training_error_response(error);
        }
    });

    CROW_ROUTE(app, "/api/training/plans/generate")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            const auto body = nlohmann::json::parse(req.body);
            training::TrainingService service{connection_pool};
            const auto task_id = service.enqueuePlanGeneration(
                user,
                body,
                context.taskScheduler(),
                context.trainingPlanGenerator()
            );
            const auto task_ref = task::task_ref_for_type("training.plan.generate", task_id);
            return api::json_response(202, {
                {"trainingTaskId", task_ref},
                {"status", "pending"}
            });
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const training::TrainingNotFound& error) {
            return api::json_response(404, {{"error", error.what()}});
        } catch (const task::TaskQueueFull& error) {
            return api::json_response(503, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return training_error_response(error);
        }
    });

    CROW_ROUTE(app, "/api/training/plans/generate/bruteforce")
    .methods(crow::HTTPMethod::Post)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            const auto body = nlohmann::json::parse(req.body);
            training::TrainingService service{connection_pool};
            const auto task_id = service.enqueueBruteForcePlanGeneration(
                user,
                body,
                context.taskScheduler(),
                context.trainingPlanGenerator()
            );
            const auto task_ref = task::task_ref_for_type("training.plan.generate.bruteforce", task_id);
            return api::json_response(202, {
                {"trainingTaskId", task_ref},
                {"status", "pending"},
                {"algorithm", "bruteforce"}
            });
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const training::TrainingNotFound& error) {
            return api::json_response(404, {{"error", error.what()}});
        } catch (const task::TaskQueueFull& error) {
            return api::json_response(503, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return training_error_response(error);
        }
    });

    CROW_ROUTE(app, "/api/training/plans")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingService service{connection_pool};
            return api::json_response(200, {{"items", service.listPlans(user.id)}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return training_error_response(error);
        }
    });

    CROW_ROUTE(app, "/api/training/plans/<int>")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req, int id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingService service{connection_pool};
            const auto detail = service.getPlanDetail(id, user.id);
            if (!detail) {
                return api::json_response(404, {{"error", "training plan not found"}});
            }
            return api::json_response(200, {
                {"plan", detail->plan},
                {"items", detail->items}
            });
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return training_error_response(error);
        }
    });

    CROW_ROUTE(app, "/api/training/plans/<int>/items")
    .methods(crow::HTTPMethod::Get)
    ([&context](const crow::request& req, int id) -> crow::response {
        try {
            auto& connection_pool = context.apiPool();
            const auto user = auth::requireUser(connection_pool, req);
            training::TrainingService service{connection_pool};
            const auto items = service.listPlanItems(id, user.id);
            if (!items) {
                return api::json_response(404, {{"error", "training plan not found"}});
            }
            return api::json_response(200, {{"items", *items}});
        } catch (const auth::Unauthorized& error) {
            return api::json_response(401, {{"error", error.what()}});
        } catch (const std::exception& error) {
            return training_error_response(error);
        }
    });
});

} // namespace
