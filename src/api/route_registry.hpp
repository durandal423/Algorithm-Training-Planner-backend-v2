#pragma once
#include "api/app.hpp"
#include "runtime/application_context.hpp"

#include <vector>

using RouteRegistrar = void (*)(App& app, runtime::ApplicationContext& context);

class RouteRegistry final {
  public:
    void add(RouteRegistrar registrar);
    void apply(App& app, runtime::ApplicationContext& context) const;

    static RouteRegistry& routeRegistry();
  private:
    std::vector<RouteRegistrar> registrars_;
};

class RouteRegistration final {
  public:
    explicit RouteRegistration(RouteRegistrar registrar);
};
