cmake_minimum_required(VERSION 3.28)
project(try-crow CXX)

include(${CMAKE_SOURCE_DIR}/cmake/LoadDotEnv.cmake)
load_dotenv(${CMAKE_SOURCE_DIR}/.env)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/config.hpp.in
    ${CMAKE_BINARY_DIR}/config.hpp
    @ONLY
)

find_package(libpqxx CONFIG REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

option(TRY_CROW_ENABLE_PCH "Enable precompiled headers for faster local rebuilds" ON)

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/api/app.hpp
    src/api/route_registry.hpp
    src/api/route_registry.cpp
    src/logging/logging.hpp
    src/logging/logging.cpp
    src/db/connection_pool.cpp
    src/db/connection_pool.hpp
    src/db/schema.hpp
    src/db/schema.cpp
    src/db/database.cpp
    src/db/database.hpp
    src/domain/hello/hello_routes.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

target_compile_features(${PROJECT_NAME} PRIVATE 
    cxx_std_23
)

if(TRY_CROW_ENABLE_PCH)
    target_precompile_headers(${PROJECT_NAME} PRIVATE
        <crow.h>
        <crow/middlewares/cors.h>
        <spdlog/spdlog.h>
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE 
    libpqxx::pqxx
    Crow::Crow
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
