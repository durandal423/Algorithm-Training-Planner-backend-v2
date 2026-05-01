#include "api/route_registry.hpp"
#include "db/connection_pool.hpp"

namespace {

RouteRegistration helloRoutesRegistration([](App &app, ConnectionPool &connection_pool) -> void {
    CROW_ROUTE(app, "/hello").methods(crow::HTTPMethod::Get)([]() {
        return "Hello, World!\n";
    });
});

} // namespace
