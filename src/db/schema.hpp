#pragma once

#include <string>

class Schema final {
public:
    Schema() = delete;

    static void ensureSchema(const std::string& migration_conninfo);
};