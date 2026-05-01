#pragma once
#include "api/app.hpp"
#include "db/connection_pool.hpp"

#include <vector>

using RouteRegistrar = void (*)(App &app, ConnectionPool &connection_pool);

class RouteRegistry final {
  public:
    void add(RouteRegistrar registrar);
    void apply(App &app, ConnectionPool &connection_pool) const;

    static RouteRegistry& routeRegistry();
  private:
    std::vector<RouteRegistrar> registrars_;
};

class RouteRegistration final {
  public:
    explicit RouteRegistration(RouteRegistrar registrar);
};
