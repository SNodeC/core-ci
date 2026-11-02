cmake_minimum_required(VERSION 3.25)
project(
    core-ci
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL 3 REQUIRED)
find_package(nlohmann_json 3.7 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(
    snodec CONFIG REQUIRED
    COMPONENTS db-mariadb http-server-express-legacy-in
               http-server-express-tls-in http-client net-in-stream-tls
)

add_library(
    controller-code STATIC
    src/Auth.cpp
    src/Graph.cpp
    src/Store.cpp
    src/Http.cpp
    src/GitHub.cpp
    src/Evidence.cpp
    src/Execution.cpp
)
target_include_directories(controller-code PUBLIC src)
target_link_libraries(
    controller-code
    PUBLIC OpenSSL::Crypto
           OpenSSL::SSL
           nlohmann_json::nlohmann_json
           ZLIB::ZLIB
           snodec::db-mariadb
           snodec::http-server-express-legacy-in
           snodec::http-server-express-tls-in
           snodec::http-client
           snodec::net-in-stream-tls
)
target_compile_options(controller-code PRIVATE -Wall -Wextra -Wpedantic)
find_program(
    MARIADBD
    NAMES mariadbd
    PATHS /usr/sbin REQUIRED
)
add_executable(core-ci src/main.cpp src/Managed.cpp)
target_link_libraries(core-ci PRIVATE controller-code)
target_compile_definitions(
    core-ci PRIVATE CORE_CI_UI_ROOT="${PROJECT_SOURCE_DIR}/ui"
                    CORE_CI_MARIADBD="${MARIADBD}"
)
target_compile_options(core-ci PRIVATE -Wall -Wextra -Wpedantic)
find_program(CLANG_TIDY NAMES clang-tidy)
if(CLANG_TIDY)
    file(GLOB CONTROLLER_CPP CONFIGURE_DEPENDS src/*.cpp)
    add_custom_target(
        analyze
        COMMAND ${CLANG_TIDY} -p ${PROJECT_BINARY_DIR} ${CONTROLLER_CPP}
        VERBATIM
    )
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(
        Python3
        COMPONENTS Interpreter
        REQUIRED
    )
    add_executable(https-contract-test tests/https.cpp)
    target_link_libraries(https-contract-test PRIVATE controller-code)
    add_test(NAME native-https-contract
             COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/https.py
                     ${PROJECT_BINARY_DIR}
    )
    add_executable(artifact-contract-test tests/artifact.cpp)
    target_link_libraries(artifact-contract-test PRIVATE controller-code)
    add_test(
        NAME artifact-bytes-contract
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/artifact.py
                ${PROJECT_BINARY_DIR}
    )
    add_test(
        NAME native-startup-http-contract
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/runtime.py
                ${PROJECT_BINARY_DIR}
    )
    set_tests_properties(
        native-startup-http-contract PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE
    )
    add_test(
        NAME native-sse-backpressure-contract
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/runtime.py
                ${PROJECT_BINARY_DIR} sse
    )
    set_tests_properties(
        native-sse-backpressure-contract PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE
    )
    add_executable(store-contract-test tests/store.cpp)
    target_link_libraries(store-contract-test PRIVATE controller-code)
    add_test(
        NAME mariadb-commit-and-restart-contract
        COMMAND
            ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/persistence.py
            ${PROJECT_BINARY_DIR}
    )
    set_tests_properties(
        mariadb-commit-and-restart-contract PROPERTIES TIMEOUT 120
    )
    add_executable(publication-contract-test tests/publication.cpp)
    target_link_libraries(publication-contract-test PRIVATE controller-code)
    add_test(
        NAME publication-confirmation-contract
        COMMAND
            ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/publication.py
            ${PROJECT_BINARY_DIR}
    )
    set_tests_properties(
        publication-confirmation-contract PROPERTIES TIMEOUT 120
    )
    add_executable(graph-contract-test tests/graph.cpp)
    target_link_libraries(graph-contract-test PRIVATE controller-code)
    add_test(NAME graph-and-receipt-contract COMMAND graph-contract-test)
    add_executable(auth-contract-test tests/auth.cpp src/Auth.cpp)
    target_include_directories(auth-contract-test PRIVATE src)
    target_link_libraries(
        auth-contract-test PRIVATE OpenSSL::Crypto nlohmann_json::nlohmann_json
    )
    target_compile_options(auth-contract-test PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME github-authentication-contract COMMAND auth-contract-test)
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(format)
file(GLOB_RECURSE TEST_CPP CONFIGURE_DEPENDS tests/*.cpp tests/*.h)
if(CLANG_FORMAT)
    add_custom_command(
        OUTPUT format-cmds
        APPEND
        COMMAND ${CLANG_FORMAT} -i ${TEST_CPP}
    )
    add_custom_target(
        format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${CHECK_CXX_SOURCE_FILES}
                ${TEST_CPP}
        VERBATIM
    )
    if(CMAKE_FORMAT)
        add_custom_command(
            TARGET format-check
            POST_BUILD
            COMMAND ${CMAKE_FORMAT} --check ${CMAKELISTS_TXT_FILES}
            VERBATIM
        )
    endif()
endif()
