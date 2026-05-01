#pragma once

#include "api/app.hpp"
#include "domain/auth/auth_models.hpp"

#include <optional>
#include <stdexcept>
#include <string>

class ConnectionPool;

namespace auth {

class Unauthorized : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Forbidden : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::string bearerToken(const crow::request& req);
std::optional<PublicUser> currentUser(ConnectionPool& pool, const crow::request& req);
PublicUser requireUser(ConnectionPool& pool, const crow::request& req);
PublicUser requireAdmin(ConnectionPool& pool, const crow::request& req);

} // namespace auth
