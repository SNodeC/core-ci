cmake_minimum_required(VERSION 3.25)
project(
    core-ci
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(OpenSSL REQUIRED)
find_package(nlohmann_json 3.7.0 REQUIRED)

find_package(
    snodec CONFIG REQUIRED COMPONENTS db-mariadb http-server-express-legacy-in
                                      http-client net-in-stream-tls
)

add_library(
    core-ci-domain STATIC src/Definition.cpp src/Version.cpp src/Graph.cpp
)
target_link_libraries(
    core-ci-domain PUBLIC nlohmann_json::nlohmann_json OpenSSL::Crypto
)
target_include_directories(core-ci-domain PUBLIC src)
target_compile_options(core-ci-domain PRIVATE -Wall -Wextra -Wpedantic)

add_executable(
    core-ci
    src/main.cpp
    src/DatabaseConfig.cpp
    src/ManagedDatabase.cpp
    src/GitHub.cpp
    src/HttpClient.cpp
    src/PrivateFile.cpp
    src/Database.cpp
    src/Enrollment.cpp
    src/UiConfig.cpp
    src/GraphStore.cpp
    src/SourceReader.cpp
    src/HttpsUrl.cpp
    src/PipelineConfig.cpp
    src/Pipeline.cpp
    src/FeedProof.cpp
)
target_link_libraries(
    core-ci
    PRIVATE core-ci-domain
            snodec::db-mariadb
            snodec::http-server-express-legacy-in
            snodec::http-client
            snodec::net-in-stream-tls
            OpenSSL::Crypto
            OpenSSL::SSL
)
file(READ ui/index.html CORE_CI_UI_HTML)
file(READ ui/style.css CORE_CI_UI_CSS)
file(READ ui/app.js CORE_CI_UI_JAVASCRIPT)
file(READ deployment/source/core-ci-build.yml.in CORE_CI_SOURCE_WORKFLOW)
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS ui/index.html ui/style.css ui/app.js
             deployment/source/core-ci-build.yml.in
)
configure_file(src/UiAssets.h.in generated/UiAssets.h @ONLY)
target_include_directories(core-ci PRIVATE "${PROJECT_BINARY_DIR}/generated")
target_compile_options(core-ci PRIVATE -Wall -Wextra -Wpedantic)

option(CORE_CI_CLANG_TIDY "Run clang-tidy during compilation" OFF)
if(CORE_CI_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy REQUIRED)
    set_property(
        TARGET core-ci core-ci-domain PROPERTY CXX_CLANG_TIDY "${CLANG_TIDY}"
    )
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(format)

file(GLOB_RECURSE CORE_CI_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp tests/*.h)
if(CLANG_FORMAT)
    if(CORE_CI_TEST_SOURCES)
        add_custom_command(
            OUTPUT format-cmds
            APPEND
            COMMAND ${CLANG_FORMAT} -i ${CORE_CI_TEST_SOURCES}
        )
    endif()
    add_custom_target(
        format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${CHECK_CXX_SOURCE_FILES}
                ${CORE_CI_TEST_SOURCES}
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

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    foreach(
        case
        help
        managed
        unmanaged
        initial-failure
        unmanaged-initial-failure
        startup-failure
        schema-failure
    )
        add_test(
            NAME lifecycle-${case}
            COMMAND
                ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/lifecycle.py
                --executable $<TARGET_FILE:core-ci> --directory
                ${PROJECT_BINARY_DIR}/tests/${case} --case ${case}
        )
        set_tests_properties(lifecycle-${case} PROPERTIES TIMEOUT 45)
    endforeach()

    add_test(
        NAME enrollment-boundaries
        COMMAND
            ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/enrollment.py
            --executable $<TARGET_FILE:core-ci> --directory
            ${PROJECT_BINARY_DIR}/tests/enrollment --graph-executable
            $<TARGET_FILE:graph-store-test>
    )
    set_tests_properties(enrollment-boundaries PROPERTIES TIMEOUT 75)

    add_executable(
        graph-store-test
        tests/GraphStoreTest.cpp src/Database.cpp src/DatabaseConfig.cpp
        src/PrivateFile.cpp src/GraphStore.cpp
    )
    target_include_directories(graph-store-test PRIVATE src)
    target_link_libraries(
        graph-store-test PRIVATE core-ci-domain snodec::db-mariadb
    )

    add_executable(graph-test tests/GraphTest.cpp)
    target_link_libraries(graph-test PRIVATE core-ci-domain)
    add_test(NAME graph-state-boundaries COMMAND graph-test)

    add_executable(definition-test tests/DefinitionTest.cpp)
    target_link_libraries(definition-test PRIVATE core-ci-domain)
    add_test(NAME definitions-and-versions
             COMMAND definition-test
                     ${PROJECT_SOURCE_DIR}/deployment/sandbox/definitions
    )
endif()

if(BUILD_TESTING)
    add_executable(
        github-test tests/GitHubTest.cpp src/GitHub.cpp src/HttpClient.cpp
                    src/PrivateFile.cpp
    )
    target_include_directories(github-test PRIVATE src)
    target_link_libraries(
        github-test
        PRIVATE nlohmann_json::nlohmann_json snodec::http-client
                snodec::net-in-stream-tls OpenSSL::SSL OpenSSL::Crypto
    )
    foreach(case success wrong-hostname wrong-ca lost-response token-rejected
                 token-lost-response
    )
        add_test(
            NAME github-transport-${case}
            COMMAND
                ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/github_transport.py --executable
                $<TARGET_FILE:github-test> --directory
                ${PROJECT_BINARY_DIR}/tests/github-${case} --case ${case}
        )
        set_tests_properties(github-transport-${case} PROPERTIES TIMEOUT 25)
    endforeach()
endif()

if(BUILD_TESTING)
    add_executable(
        source-reader-test
        tests/SourceReaderTest.cpp src/SourceReader.cpp src/GitHub.cpp
        src/HttpClient.cpp src/PrivateFile.cpp
    )
    target_include_directories(source-reader-test PRIVATE src)
    target_link_libraries(
        source-reader-test PRIVATE core-ci-domain snodec::http-client
                                   snodec::net-in-stream-tls OpenSSL::SSL
    )
    foreach(
        case
        success
        ambiguous
        no-tags
        symlink
        modified-blob
        invalid-definition
        invalid-tag
        duplicate-grant
    )
        add_test(
            NAME source-selection-${case}
            COMMAND
                ${Python3_EXECUTABLE}
                ${PROJECT_SOURCE_DIR}/tests/source_selection.py --executable
                $<TARGET_FILE:source-reader-test> --directory
                ${PROJECT_BINARY_DIR}/tests/source-${case} --case ${case}
        )
        set_tests_properties(source-selection-${case} PROPERTIES TIMEOUT 30)
    endforeach()
endif()

if(BUILD_TESTING)
    add_test(
        NAME pipeline-boundaries
        COMMAND
            ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/pipeline.py
            --executable $<TARGET_FILE:core-ci> --directory
            ${PROJECT_BINARY_DIR}/tests/pipeline
    )
    set_tests_properties(pipeline-boundaries PROPERTIES TIMEOUT 75)
endif()

if(BUILD_TESTING)
    add_test(
        NAME sandbox-hook-boundaries
        COMMAND
            ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/sandbox_hooks.py
            ${PROJECT_BINARY_DIR}/tests/sandbox-hooks
    )
    set_tests_properties(sandbox-hook-boundaries PROPERTIES TIMEOUT 120)
    add_test(
        NAME openwrt-archive-boundaries
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/openwrt.py
                ${PROJECT_BINARY_DIR}/tests/openwrt
    )
    add_test(
        NAME worker-environment-boundaries
        COMMAND
            ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/worker_environment.py
            ${PROJECT_BINARY_DIR}/tests/worker-environment
    )
    add_test(
        NAME worker-package-boundaries
        COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tests/worker.py
                --directory ${PROJECT_BINARY_DIR}/tests/worker
    )
    set_tests_properties(worker-package-boundaries PROPERTIES TIMEOUT 120)
    add_test(
        NAME worker-provenance-boundaries
        COMMAND
            ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/worker_provenance.py --directory
            ${PROJECT_BINARY_DIR}/tests/worker-provenance
    )
    set_tests_properties(worker-provenance-boundaries PROPERTIES TIMEOUT 15)
endif()
