#include "api/route_registry.hpp"
#include "db/database.hpp"
#include "domain/auth/bootstrap_admin.hpp"
#include "runtime/application_context.hpp"

#include "config.hpp"

#include <spdlog/spdlog.h>

int main() {
    try {
        initSpdlog();
        installCrowLogger();

        Database::ensureDatabase();
        Database::ensureSchema();

        runtime::ApplicationContext application_context{Database::appConninfo()};
        auth::bootstrapAdminFromEnv(application_context.apiPool());

        App app;
        auto &cors = app.get_middleware<crow::CORSHandler>();
        cors.global()
            .origin("*")
            .methods(
                crow::HTTPMethod::Get,
                crow::HTTPMethod::Post,
                crow::HTTPMethod::Put,
                crow::HTTPMethod::Delete,
                crow::HTTPMethod::Options)
            .headers("Content-Type", "Authorization")
            .max_age(600);

        RouteRegistry::routeRegistry().apply(app, application_context);

        spdlog::info("server starting at http://0.0.0.0:{}", config::SERVER_PORT);
        app.port(config::SERVER_PORT).multithreaded().run();

        spdlog::shutdown();
        return 0;
    } catch (const std::exception &error) {
        spdlog::error("startup failed: {}", error.what());
        spdlog::shutdown();
        return 1;
    }
}
