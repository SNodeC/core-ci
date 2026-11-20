cmake_minimum_required(VERSION 3.20)
include(cmake/Version.cmake)
project(
    coreci
    VERSION ${RELEASE_VERSION}
    LANGUAGES CXX
)
include(GNUInstallDirs)
option(CORECI_WITH_SNODEC "Build SNode.C adapters (introduced in Phase 3)" OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(nlohmann_json REQUIRED)
add_library(
    coreci_core core/Version.cpp core/Graph.cpp core/WaveController.cpp
                core/PublicationController.cpp core/Status.cpp core/Badge.cpp
)
target_include_directories(coreci_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(coreci_core PUBLIC -Wall -Wextra -Wpedantic -Werror)
add_library(
    coreci_persistence persistence/Input.cpp persistence/StateSession.cpp
                       persistence/StateSql.cpp
)
target_link_libraries(coreci_persistence PUBLIC coreci_core)
add_library(coreci_status app/StatusJson.cpp)
target_link_libraries(
    coreci_status PUBLIC coreci_core nlohmann_json::nlohmann_json
)
add_library(
    coreci_legacy
    migration/legacy/Engine.cpp
    migration/legacy/Status.cpp
    migration/legacy/Badge.cpp
    migration/legacy/StatusJson.cpp
    migration/legacy/EventRow.cpp
    migration/legacy/EventSql.cpp
    migration/Convert.cpp
)
target_link_libraries(coreci_legacy PUBLIC coreci_core)
add_library(coreci_legacy_test tests/InMemoryEventStore.cpp)
target_link_libraries(coreci_legacy_test PUBLIC coreci_legacy)
add_library(coreci_test tests/InMemoryStateStore.cpp)
target_link_libraries(coreci_test PUBLIC coreci_status coreci_persistence)
enable_testing()
foreach(scenario S06_Tags S09_Validation S12_WriteAhead)
    add_executable(${scenario} tests/${scenario}.cpp)
    target_link_libraries(${scenario} PRIVATE coreci_test)
    add_test(NAME ${scenario} COMMAND ${scenario})
endforeach()
set_tests_properties(S06_Tags S09_Validation PROPERTIES LABELS core)
set_tests_properties(S12_WriteAhead PROPERTIES LABELS persistence)
add_executable(W_State tests/W_State.cpp)
target_link_libraries(W_State PRIVATE coreci_test)
foreach(scenario early failures outcomes deadlines dispatch randomized)
    add_test(NAME State_${scenario} COMMAND W_State ${scenario})
    set_tests_properties(State_${scenario} PROPERTIES LABELS "core;persistence")
endforeach()
foreach(scenario S13_EventRow S14_EventSql)
    add_executable(${scenario} tests/${scenario}.cpp)
    target_link_libraries(${scenario} PRIVATE coreci_legacy_test)
    add_test(NAME ${scenario} COMMAND ${scenario})
    set_tests_properties(${scenario} PROPERTIES LABELS migration)
endforeach()
add_executable(W_Engine tests/W_Engine.cpp)
target_link_libraries(W_Engine PRIVATE coreci_legacy_test)
foreach(
    scenario
    debounce
    planning
    outcomes
    retry
    dispatch
    restart
    late
    deadlines
    graph
    randomized
)
    add_test(NAME W_${scenario} COMMAND W_Engine ${scenario})
    set_tests_properties(W_${scenario} PROPERTIES LABELS migration)
endforeach()
if(CORECI_WITH_SNODEC)
    find_package(
        snodec REQUIRED COMPONENTS db-mariadb http-client net-in-stream-tls
                                   http-server-express-tls-in
    )
    find_package(OpenSSL REQUIRED)
    find_package(snodec-auth 2 REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME A_SlimSource
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/slim_source.py
            $<TARGET_FILE:core-ci>
    )
    set_tests_properties(A_SlimSource PROPERTIES LABELS adapters)
    add_library(
        coreci_github
        adapters/github/Protocol.cpp adapters/github/HttpClient.cpp
        adapters/github/Declarations.cpp adapters/github/AppToken.cpp
    )
    add_executable(W_ReplayHistory tests/W_ReplayHistory.cpp)
    target_link_libraries(
        W_ReplayHistory PRIVATE coreci_legacy coreci_status coreci_persistence
    )
    add_test(NAME W_ReplayHistory
             COMMAND W_ReplayHistory ${CMAKE_CURRENT_SOURCE_DIR}/tests/data
    )
    set_tests_properties(
        W_ReplayHistory PROPERTIES LABELS "migration;persistence"
    )
    target_link_libraries(
        coreci_github
        PUBLIC coreci_persistence snodec::http-client snodec::net-in-stream-tls
               OpenSSL::SSL OpenSSL::Crypto
    )
    file(READ app/dashboard/index.html DASHBOARD_HTML)
    file(READ app/dashboard/style.css DASHBOARD_CSS)
    file(READ app/dashboard/app.js DASHBOARD_JS)
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS app/dashboard/index.html
                 app/dashboard/style.css app/dashboard/app.js
    )
    configure_file(app/DashboardAssets.h.in generated/DashboardAssets.h @ONLY)
    add_library(
        coreci_app
        app/App.cpp app/Config.cpp app/ApiRouter.cpp app/WebhookRouter.cpp
        app/Dashboard.cpp adapters/dashboard/Model.cpp
    )
    target_include_directories(
        coreci_app PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_link_libraries(
        coreci_app
        PUBLIC coreci_status coreci_github coreci_mariadb
               snodec::http-server-express-tls-in snodec-auth::auth-client
    )
    add_executable(W_Model tests/W_Model.cpp)
    target_link_libraries(W_Model PRIVATE coreci_app coreci_test)
    add_test(NAME W_Model COMMAND W_Model)
    set_tests_properties(W_Model PROPERTIES LABELS adapters)
    add_library(coreci-test-clock SHARED tests/dashboard/Clock.cpp)
    target_link_libraries(coreci-test-clock PRIVATE ${CMAKE_DL_LIBS})
    foreach(mode stream buttons auth renewal)
        if(mode STREQUAL "stream")
            set(case S22_DashboardStream)
        elseif(mode STREQUAL "buttons")
            set(case S23_DashboardButtons)
        elseif(mode STREQUAL "auth")
            set(case S24_DashboardAuth)
        else()
            set(case S25_DashboardRenewal)
        endif()
        add_test(
            NAME ${case}
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/dashboard/checks.py
                ${CMAKE_BINARY_DIR} ${mode}
        )
        set_tests_properties(
            ${case}
            PROPERTIES LABELS
                       "integration;mariadb"
                       ENVIRONMENT
                       "PYTHONDONTWRITEBYTECODE=1"
                       SKIP_RETURN_CODE
                       77
                       RUN_SERIAL
                       TRUE
                       TIMEOUT
                       150
        )
    endforeach()
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
    add_executable(coreci-interrupt-store tests/InterruptStore.cpp)
    target_link_libraries(coreci-interrupt-store PRIVATE coreci_mariadb)
    add_executable(coreci-reset-store tests/ResetStore.cpp)
    target_link_libraries(coreci-reset-store PRIVATE coreci_mariadb)
    add_test(
        NAME M_LocalSmoke
        COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.py ${CMAKE_BINARY_DIR}
    )
    add_test(
        NAME S26_WaveStart
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/wave_start.py ${CMAKE_BINARY_DIR}
    )
    add_test(
        NAME S27_LatePublication
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/late_publication.py
            ${CMAKE_BINARY_DIR}
    )
    foreach(
        mode
        append
        result-before
        result-after
        restart-dispatch
        publication-dispatch
        load
        slow
        shutdown
        shutdown-idle
        shutdown-int
    )
        add_test(
            NAME ConnectionLoss-${mode}
            COMMAND
                ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/connection_loss.py
                ${CMAKE_BINARY_DIR} ${mode}
        )
        set_tests_properties(
            ConnectionLoss-${mode}
            PROPERTIES LABELS "integration;mariadb" TIMEOUT 90 ENVIRONMENT
                       "PYTHONDONTWRITEBYTECODE=1"
        )
    endforeach()
    set_tests_properties(
        M_LocalSmoke S26_WaveStart S27_LatePublication
        PROPERTIES LABELS
                   "integration;mariadb"
                   SKIP_RETURN_CODE
                   77
                   TIMEOUT
                   150
                   RUN_SERIAL
                   TRUE
    )
    foreach(
        scenario
        A_WebhookHmac
        A_CallbackHmac
        A_DispatchRequest
        A_TagPages
        S16_Declarations
        S17_ProductionContract
        A_AppToken
    )
        add_executable(${scenario} tests/${scenario}.cpp)
        target_link_libraries(${scenario} PRIVATE coreci_github coreci_test)
        add_test(NAME ${scenario} COMMAND ${scenario})
        set_tests_properties(${scenario} PROPERTIES LABELS adapters)
    endforeach()
    target_compile_definitions(
        S16_Declarations
        PRIVATE CORECI_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data"
    )
    target_compile_definitions(
        S17_ProductionContract
        PRIVATE
            CORECI_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/tests/data/production"
    )
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS adapters/mariadb/Schema.sql
    )
    file(READ adapters/mariadb/Schema.sql CORECI_SCHEMA)
    configure_file(adapters/mariadb/Schema.h.in generated/Schema.h @ONLY)
    add_library(
        coreci_legacy_mariadb migration/legacy/MariaDBEventStore.cpp
                              migration/Install.cpp
    )
    target_include_directories(
        coreci_legacy_mariadb PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_link_libraries(
        coreci_legacy_mariadb PUBLIC coreci_legacy coreci_persistence
                                     snodec::db-mariadb
    )
    add_executable(coreci-migrate migration/main.cpp app/Config.cpp)
    target_link_libraries(coreci-migrate PRIVATE coreci_legacy_mariadb)
    install(TARGETS coreci-migrate RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    add_library(coreci_mariadb adapters/mariadb/MariaDBStateStore.cpp)
    target_include_directories(
        coreci_mariadb PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_link_libraries(
        coreci_mariadb PUBLIC coreci_persistence snodec::db-mariadb
    )
    add_executable(coreci-state-dump tests/StateDump.cpp)
    target_link_libraries(
        coreci-state-dump PRIVATE coreci_mariadb coreci_legacy coreci_status
    )
    add_test(
        NAME M_Migration
        COMMAND
            ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/migration.py ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(
        M_Migration
        PROPERTIES LABELS
                   "integration;mariadb"
                   RUN_SERIAL
                   TRUE
                   TIMEOUT
                   120
                   ENVIRONMENT
                   "PYTHONDONTWRITEBYTECODE=1"
    )
    foreach(scenario A_MariaDBRows M_MariaDBIntegration)
        add_executable(${scenario} tests/${scenario}.cpp)
        target_link_libraries(${scenario} PRIVATE coreci_mariadb coreci_test)
        add_test(NAME ${scenario} COMMAND ${scenario})
    endforeach()
    set_tests_properties(A_MariaDBRows PROPERTIES LABELS adapters)
    set_tests_properties(
        M_MariaDBIntegration PROPERTIES LABELS "integration;mariadb"
                                        SKIP_RETURN_CODE 77 TIMEOUT 30
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
    adapters/dashboard/*.h
    adapters/dashboard/*.cpp
    adapters/mariadb/*.h
    adapters/mariadb/*.cpp
    migration/*.h
    migration/*.cpp
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
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Graph.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/WaveController.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/PublicationController.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Status.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Badge.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/Input.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/StateSession.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/StateSql.cpp
    )
endif()
