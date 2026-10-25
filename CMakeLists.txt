cmake_minimum_required(VERSION 3.25)
project(
    core-ci
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(
    snodec CONFIG REQUIRED COMPONENTS db-mariadb http-server-express-legacy-in
)

add_executable(
    core-ci src/main.cpp src/DatabaseConfig.cpp src/ManagedDatabase.cpp
)
target_link_libraries(
    core-ci PRIVATE snodec::db-mariadb snodec::http-server-express-legacy-in
)
target_compile_options(core-ci PRIVATE -Wall -Wextra -Wpedantic)

option(CORE_CI_CLANG_TIDY "Run clang-tidy during compilation" OFF)
if(CORE_CI_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy REQUIRED)
    set_property(TARGET core-ci PROPERTY CXX_CLANG_TIDY "${CLANG_TIDY}")
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
endif()

if(BUILD_TESTING)
    foreach(case help managed unmanaged initial-failure
                 unmanaged-initial-failure startup-failure
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
endif()
