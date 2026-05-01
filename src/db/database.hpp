#pragma once
#include <pqxx/pqxx>

#include <string>

class Database {
public:
    static std::string adminConninfo();
    static std::string appConninfo();

    static void ensureDatabase();
    static void ensureSchema();
};