#include "db/connection_pool.hpp"
#include "domain/auth/auth_repository.hpp"
#include "domain/auth/auth_service.hpp"
#include "test_support/db_test_utils.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("auth repository stores hashed sessions for current user lookup") {
    if (!test_support::dbTestsEnabled()) {
        SUCCEED("Set RUN_DB_TESTS=1 to run PostgreSQL auth repository tests.");
        return;
    }

    test_support::ensureDatabaseReady();
    const auto username = test_support::uniqueName("db_auth_user_");
    const auto email = username + "@example.test";

    struct Cleanup {
        std::string username;
        ~Cleanup() {
            try {
                test_support::deleteUserByUsername(username);
            } catch (...) {
            }
        }
    } cleanup{username};

    ConnectionPool pool{Database::appConninfo(), 4};
    auth::AuthRepository repository{pool};
    auth::AuthService service{repository};

    const auto registered = service.registerUser(
        username,
        email,
        username,
        "password-123"
    );

    const auto current = service.currentUser(registered.token);
    REQUIRE(current.has_value());
    REQUIRE(current->id == registered.user.id);
    REQUIRE(current->username == username);

    const auto token_hash = auth::AuthService::hashToken(registered.token);
    const auto by_session = repository.findUserByActiveSession(token_hash);
    REQUIRE(by_session.has_value());
    REQUIRE(by_session->id == registered.user.id);

    REQUIRE(service.logout(registered.token));
    REQUIRE_FALSE(service.currentUser(registered.token).has_value());
}
