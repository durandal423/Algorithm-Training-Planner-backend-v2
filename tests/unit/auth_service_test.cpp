#include "domain/auth/auth_service.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("password hash verifies the original password only") {
    const std::string password = "correct-horse-123";
    const auto hash = auth::AuthService::hashPassword(password);

    REQUIRE(hash != password);
    REQUIRE(auth::AuthService::verifyPassword(hash, password));
    REQUIRE_FALSE(auth::AuthService::verifyPassword(hash, "wrong-password"));
}

TEST_CASE("token hash is deterministic and does not expose the token") {
    const auto token = auth::AuthService::generateToken();
    const auto first_hash = auth::AuthService::hashToken(token);
    const auto second_hash = auth::AuthService::hashToken(token);

    REQUIRE_FALSE(token.empty());
    REQUIRE(first_hash == second_hash);
    REQUIRE(first_hash != token);
    REQUIRE(first_hash != auth::AuthService::hashToken(token + "x"));
}
