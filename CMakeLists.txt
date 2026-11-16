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
add_library(
    coreci_core core/Version.cpp core/Graph.cpp core/Engine.cpp core/Status.cpp
                core/Badge.cpp
)
target_include_directories(coreci_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(coreci_core PUBLIC -Wall -Wextra -Wpedantic -Werror)
add_library(
    coreci_persistence persistence/EventRow.cpp persistence/EventQueue.cpp
                       persistence/EventSql.cpp
)
target_link_libraries(coreci_persistence PUBLIC coreci_core)
add_library(coreci_status app/StatusJson.cpp)
target_link_libraries(coreci_status PUBLIC coreci_core)
add_library(coreci_test tests/InMemoryEventStore.cpp)
target_link_libraries(coreci_test PUBLIC coreci_status coreci_persistence)
enable_testing()
foreach(scenario S06_Tags S09_Validation S12_WriteAhead S13_EventRow
                 S14_EventSql
)
    add_executable(${scenario} tests/${scenario}.cpp)
    target_link_libraries(${scenario} PRIVATE coreci_test)
    add_test(NAME ${scenario} COMMAND ${scenario})
endforeach()
set_tests_properties(S06_Tags S09_Validation PROPERTIES LABELS core)
set_tests_properties(
    S12_WriteAhead S13_EventRow S14_EventSql PROPERTIES LABELS persistence
)
add_executable(W_Engine tests/W_Engine.cpp)
target_link_libraries(W_Engine PRIVATE coreci_test)
foreach(
    scenario
    debounce
    planning
    outcomes
    retry
    dispatch
    restart
    graph
    randomized
)
    add_test(NAME W_${scenario} COMMAND W_Engine ${scenario})
    set_tests_properties(W_${scenario} PROPERTIES LABELS "core;persistence")
endforeach()
if(CORECI_WITH_SNODEC)
    find_package(
        snodec REQUIRED COMPONENTS db-mariadb http-client net-in-stream-tls
                                   http-server-express-tls-in
    )
    find_package(OpenSSL REQUIRED)
    find_package(snodec-auth 2 REQUIRED)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME A_SlimSource
             COMMAND ${Python3_EXECUTABLE}
                     ${CMAKE_CURRENT_SOURCE_DIR}/tests/slim_source.py
    )
    set_tests_properties(A_SlimSource PROPERTIES LABELS adapters)
    add_library(
        coreci_github
        adapters/github/Protocol.cpp adapters/github/HttpClient.cpp
        adapters/github/Declarations.cpp adapters/github/AppToken.cpp
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
        coreci_app app/App.cpp app/Dashboard.cpp adapters/dashboard/Model.cpp
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
                       integration
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
    foreach(
        mode
        append
        result-before
        result-after
        restart-dispatch
        replay
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
            PROPERTIES LABELS integration TIMEOUT 90 ENVIRONMENT
                       "PYTHONDONTWRITEBYTECODE=1"
        )
    endforeach()
    set_tests_properties(
        M_LocalSmoke S26_WaveStart
        PROPERTIES LABELS
                   integration
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
    add_library(coreci_mariadb adapters/mariadb/MariaDBEventStore.cpp)
    target_include_directories(
        coreci_mariadb PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated
    )
    target_link_libraries(
        coreci_mariadb PUBLIC coreci_persistence snodec::db-mariadb
    )
    foreach(scenario A_MariaDBRows M_MariaDBIntegration)
        add_executable(${scenario} tests/${scenario}.cpp)
        target_link_libraries(${scenario} PRIVATE coreci_mariadb coreci_test)
        add_test(NAME ${scenario} COMMAND ${scenario})
    endforeach()
    set_tests_properties(A_MariaDBRows PROPERTIES LABELS adapters)
    set_tests_properties(
        M_MariaDBIntegration PROPERTIES LABELS integration SKIP_RETURN_CODE 77
                                        TIMEOUT 30
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
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Engine.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Status.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/core/Badge.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/EventRow.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/EventQueue.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/persistence/EventSql.cpp
    )
endif()
