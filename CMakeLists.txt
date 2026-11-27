cmake_minimum_required(VERSION 3.20)
include(cmake/Version.cmake)
project(
    coreci
    VERSION ${RELEASE_VERSION}
    LANGUAGES CXX
)
include(GNUInstallDirs)

option(CORECI_WITH_SNODEC "Build SNode.C adapters" OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)
enable_testing()

add_library(
    coreci_core core/Version.cpp core/Scheduler.cpp core/Retention.cpp
                core/Badge.cpp
)
target_include_directories(coreci_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(coreci_core PUBLIC -Wall -Wextra -Wpedantic -Werror)
target_link_libraries(
    coreci_core PUBLIC nlohmann_json::nlohmann_json OpenSSL::Crypto
)
add_library(
    coreci_persistence persistence/StateSession.cpp persistence/StateSql.cpp
)
target_link_libraries(coreci_persistence PUBLIC coreci_core)

add_executable(X_Scheduler tests/X_Scheduler.cpp)
target_link_libraries(X_Scheduler PRIVATE coreci_persistence)
foreach(
    mode
    turns
    binding
    attempts
    publications
    retention
    expiry
    persistence
)
    add_test(NAME X_${mode} COMMAND X_Scheduler ${mode})
endforeach()
if(CORECI_WITH_SNODEC)
    find_package(
        snodec REQUIRED COMPONENTS db-mariadb http-client net-in-stream-tls
                                   http-server-express-tls-in
    )
    find_package(snodec-auth 2 REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_library(
        coreci_github
        adapters/github/Protocol.cpp adapters/github/HttpClient.cpp
        adapters/github/Declarations.cpp adapters/github/AppToken.cpp
    )
    target_link_libraries(
        coreci_github PUBLIC coreci_core snodec::http-client
                             snodec::net-in-stream-tls OpenSSL::SSL
    )

    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS adapters/mariadb/Schema.sql
                 app/dashboard/index.html app/dashboard/style.css
                 app/dashboard/app.js
    )
    file(READ adapters/mariadb/Schema.sql CORECI_SCHEMA)
    configure_file(adapters/mariadb/Schema.h.in generated/Schema.h @ONLY)
    file(READ app/dashboard/index.html DASHBOARD_HTML)
    file(READ app/dashboard/style.css DASHBOARD_CSS)
    file(READ app/dashboard/app.js DASHBOARD_JS)
    configure_file(app/DashboardAssets.h.in generated/DashboardAssets.h @ONLY)

    add_library(coreci_mariadb adapters/mariadb/MariaDBStateStore.cpp)
    target_include_directories(
        coreci_mariadb PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_link_libraries(
        coreci_mariadb PUBLIC coreci_persistence snodec::db-mariadb
    )
    add_library(
        coreci_app app/App.cpp app/Config.cpp app/ApiRouter.cpp
                   app/WebhookRouter.cpp app/Dashboard.cpp
    )
    target_include_directories(
        coreci_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_link_libraries(
        coreci_app
        PUBLIC coreci_github coreci_mariadb snodec::http-server-express-tls-in
               snodec-auth::auth-client
    )
    add_executable(core-ci app/main.cpp)
    target_link_libraries(core-ci PRIVATE coreci_app)
    install(TARGETS core-ci RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES app/dashboard/index.html app/dashboard/style.css
                  app/dashboard/app.js
            DESTINATION ${CMAKE_INSTALL_DATADIR}/core-ci/dashboard
    )
    install(FILES adapters/mariadb/Schema.sql
            DESTINATION ${CMAKE_INSTALL_DATADIR}/core-ci
    )

    foreach(scenario A_WebhookHmac A_CallbackHmac A_TagPages A_AppToken)
        add_executable(${scenario} tests/${scenario}.cpp)
        target_link_libraries(${scenario} PRIVATE coreci_github)
        add_test(NAME ${scenario} COMMAND ${scenario})
        set_tests_properties(${scenario} PROPERTIES LABELS adapters)
    endforeach()
    add_test(
        NAME A_SlimSource
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/slim_source.py
            $<TARGET_FILE:core-ci>
    )
    set_tests_properties(A_SlimSource PROPERTIES LABELS adapters)
    add_test(
        NAME X_Http
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/contract_x.py ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(
        X_Http
        PROPERTIES LABELS
                   "integration;mariadb"
                   RUN_SERIAL
                   TRUE
                   TIMEOUT
                   150
                   ENVIRONMENT
                   "PYTHONDONTWRITEBYTECODE=1"
    )
endif()
# Explicit discovery stays within owned source directories.
file(
    GLOB_RECURSE
    CORECI_FORMAT_SOURCES
    CONFIGURE_DEPENDS
    core/*.h
    core/*.cpp
    persistence/*.h
    persistence/*.cpp
    adapters/github/*.h
    adapters/github/*.cpp
    app/*.h
    app/*.cpp
    adapters/mariadb/*.h
    adapters/mariadb/*.cpp
    tests/*.h
    tests/*.cpp
)
find_program(CLANG_FORMAT clang-format)
find_program(CMAKE_FORMAT cmake-format)
if(CLANG_FORMAT AND CMAKE_FORMAT)
    file(GLOB CORECI_FORMAT_CMAKE CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.cmake"
    )
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT} -i ${CORECI_FORMAT_SOURCES}
        COMMAND ${CMAKE_FORMAT} -i ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
                ${CORECI_FORMAT_CMAKE}
    )
    add_custom_target(
        format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${CORECI_FORMAT_SOURCES}
        COMMAND
            ${CMAKE_FORMAT} --check ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
            ${CORECI_FORMAT_CMAKE}
    )
endif()
include(cmake/Packing.cmake)
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    add_custom_target(
        analyze
        COMMAND
            ${CLANG_TIDY} -p ${CMAKE_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Version.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Scheduler.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Retention.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Badge.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/StateSession.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/StateSql.cpp
    )
endif()
