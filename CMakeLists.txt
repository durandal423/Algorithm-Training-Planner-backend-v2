cmake_minimum_required(VERSION 3.28)
project(Algorithm-Training-Planner-backend CXX)

include(${CMAKE_SOURCE_DIR}/cmake/LoadDotEnv.cmake)
load_dotenv(${CMAKE_SOURCE_DIR}/.env)

configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/config.hpp.in
    ${CMAKE_BINARY_DIR}/config.hpp
    @ONLY
)

find_package(libpqxx CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(Crow CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Catch2 CONFIG REQUIRED)

option(TRY_CROW_ENABLE_PCH "Enable precompiled headers for faster local rebuilds" ON)

# This project does not use C++20 modules. Newer CMake versions scan C++20+
# targets by default, which requires clang-scan-deps when building with Clang.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/api/app.hpp
    src/api/http_json.hpp
    src/api/http_json.cpp
    src/api/route_registry.hpp
    src/api/route_registry.cpp
    src/algorithms/training_plan_algorithms.cpp
    src/algorithms/training_plan_algorithms.hpp
    src/concurrency/thread_pool.hpp
    src/concurrency/thread_pool.cpp
    src/domain/task/task_models.hpp
    src/domain/task/task_repository.hpp
    src/domain/task/task_repository.cpp
    src/domain/task/task_scheduler.hpp
    src/domain/task/task_scheduler.cpp
    src/runtime/application_context.hpp
    src/runtime/application_context.cpp
    src/domain/task/task_routes.cpp
    src/logging/logging.hpp
    src/logging/logging.cpp
    src/db/connection_pool.cpp
    src/db/connection_pool.hpp
    src/db/schema.hpp
    src/db/schema.cpp
    src/db/database.cpp
    src/db/database.hpp
    src/domain/hello/hello_routes.cpp
    src/domain/auth/auth_http.cpp
    src/domain/auth/auth_http.hpp
    src/domain/auth/bootstrap_admin.cpp
    src/domain/auth/bootstrap_admin.hpp
    src/domain/auth/auth_models.hpp
    src/domain/auth/auth_repository.cpp
    src/domain/auth/auth_repository.hpp
    src/domain/auth/auth_routes.cpp
    src/domain/auth/auth_service.cpp
    src/domain/auth/auth_service.hpp
    src/domain/problem/problem_models.hpp
    src/domain/problem/problem_repository.cpp
    src/domain/problem/problem_repository.hpp
    src/domain/problem/problem_routes.cpp
    src/domain/training/training_models.hpp
    src/domain/training/training_repository.cpp
    src/domain/training/training_repository.hpp
    src/domain/training/training_service.cpp
    src/domain/training/training_service.hpp
    src/domain/training/training_plan_generation_service.cpp
    src/domain/training/training_plan_generation_service.hpp
    src/domain/training/training_routes.cpp
    src/domain/judge/judge_models.hpp
    src/domain/judge/judge_repository.cpp
    src/domain/judge/judge_repository.hpp
    src/domain/judge/judge_routes.cpp
    src/domain/admin/admin_models.hpp
    src/domain/admin/admin_repository.cpp
    src/domain/admin/admin_repository.hpp
    src/domain/admin/admin_routes.cpp
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
    unofficial-sodium::sodium
    Crow::Crow
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

add_executable(Algorithm-judge-worker
    src/judge_worker_main.cpp
    src/logging/logging.hpp
    src/logging/logging.cpp
    src/db/connection_pool.cpp
    src/db/connection_pool.hpp
    src/db/schema.hpp
    src/db/schema.cpp
    src/db/database.cpp
    src/db/database.hpp
    src/domain/judge/judge_dispatcher.cpp
    src/domain/judge/judge_dispatcher.hpp
    src/domain/judge/judge_models.hpp
    src/domain/judge/judge_repository.cpp
    src/domain/judge/judge_repository.hpp
    src/domain/judge/judge_service.cpp
    src/domain/judge/judge_service.hpp
    src/domain/judge/output_comparator.cpp
    src/domain/judge/output_comparator.hpp
    src/domain/judge/docker_sandbox_pool.cpp
    src/domain/judge/docker_sandbox_pool.hpp
    src/domain/judge/sandbox_process.cpp
    src/domain/judge/sandbox_process.hpp
    src/domain/judge/sandbox_runner.cpp
    src/domain/judge/sandbox_runner.hpp
)

target_include_directories(Algorithm-judge-worker PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}
)

target_compile_features(Algorithm-judge-worker PRIVATE
    cxx_std_23
)

target_link_libraries(Algorithm-judge-worker PRIVATE
    libpqxx::pqxx
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

enable_testing()

add_executable(project-tests
    tests/unit/auth_service_test.cpp
    tests/unit/output_comparator_test.cpp
    tests/unit/training_plan_algorithms_test.cpp
    tests/db/auth_repository_test.cpp
    tests/db/judge_repository_queue_test.cpp
    tests/db/task_repository_test.cpp
    src/algorithms/training_plan_algorithms.cpp
    src/algorithms/training_plan_algorithms.hpp
    src/db/connection_pool.cpp
    src/db/connection_pool.hpp
    src/db/database.cpp
    src/db/database.hpp
    src/db/schema.cpp
    src/db/schema.hpp
    src/domain/auth/auth_models.hpp
    src/domain/auth/auth_repository.cpp
    src/domain/auth/auth_repository.hpp
    src/domain/auth/auth_service.cpp
    src/domain/auth/auth_service.hpp
    src/domain/judge/judge_models.hpp
    src/domain/judge/judge_repository.cpp
    src/domain/judge/judge_repository.hpp
    src/domain/judge/output_comparator.cpp
    src/domain/judge/output_comparator.hpp
    src/domain/training/training_models.hpp
    src/domain/task/task_models.hpp
    src/domain/task/task_repository.cpp
    src/domain/task/task_repository.hpp
)

target_include_directories(project-tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests
    ${CMAKE_BINARY_DIR}
)

target_compile_features(project-tests PRIVATE
    cxx_std_23
)

target_link_libraries(project-tests PRIVATE
    Catch2::Catch2WithMain
    libpqxx::pqxx
    unofficial-sodium::sodium
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

include(Catch)
catch_discover_tests(project-tests)
