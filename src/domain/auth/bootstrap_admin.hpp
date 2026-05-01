#pragma once

class ConnectionPool;

namespace auth {

void bootstrapAdminFromEnv(ConnectionPool& pool);

} // namespace auth
