#include "api/route_registry.hpp"

void RouteRegistry::add(RouteRegistrar registrar) {
    registrars_.push_back(registrar);
}

void RouteRegistry::apply(App &app, ConnectionPool &connection_pool) const {
    for (auto registrar : registrars_) {
        registrar(app, connection_pool);
    }
}

RouteRegistry& RouteRegistry::routeRegistry() {
    static RouteRegistry registry;
    return registry;
}

RouteRegistration::RouteRegistration(RouteRegistrar registrar) {
    RouteRegistry::routeRegistry().add(registrar);
}
