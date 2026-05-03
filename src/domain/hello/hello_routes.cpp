#include "api/route_registry.hpp"

namespace {

RouteRegistration helloRoutesRegistration([](App& app, runtime::ApplicationContext&) -> void {
    CROW_ROUTE(app, "/hello").methods(crow::HTTPMethod::Get)([]() {
        return "Hello, World!\n";
    });
});

} // namespace
