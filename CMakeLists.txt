cmake_minimum_required(VERSION 3.20)
project(coreci LANGUAGES CXX)
option(CORECI_WITH_SNODEC "Build SNode.C adapters (introduced in Phase 3)" OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_library(
    coreci_core core/Version.cpp core/Graph.cpp core/Engine.cpp core/Status.cpp
)
target_include_directories(coreci_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(coreci_core PUBLIC -Wall -Wextra -Wpedantic -Werror)
enable_testing()
foreach(
    scenario
    S01_Diamond
    S02_PublishSerialization
    S03_Superseding
    S04_FailureRetry
    S05_Idempotency
    S06_Tags
    S07_DependencyChange
    S08_WaitingTag
    S09_Validation
    S10_Timeouts
    I_RandomizedInvariants
)
    add_executable(${scenario} tests/${scenario}.cpp)
    target_link_libraries(${scenario} PRIVATE coreci_core)
    add_test(NAME ${scenario} COMMAND ${scenario})
    set_tests_properties(${scenario} PROPERTIES LABELS core)
endforeach()
# The canonical modules assume src/. This layout uses the same committed styles
# with explicit discovery limited to core/ and tests/.
file(
    GLOB_RECURSE
    CORECI_FORMAT_SOURCES
    CONFIGURE_DEPENDS
    core/*.h
    core/*.cpp
    tests/*.h
    tests/*.cpp
)
find_program(CLANG_FORMAT clang-format)
find_program(CMAKE_FORMAT cmake-format)
if(CLANG_FORMAT AND CMAKE_FORMAT)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT} -i ${CORECI_FORMAT_SOURCES}
        COMMAND ${CMAKE_FORMAT} -i ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    )
    add_custom_target(
        format-check
        COMMAND ${CLANG_FORMAT} --dry-run --Werror ${CORECI_FORMAT_SOURCES}
        COMMAND ${CMAKE_FORMAT} --check
                ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    )
endif()
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
    )
endif()
