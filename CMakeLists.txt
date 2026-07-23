cmake_minimum_required(VERSION 3.10)
project(RISCVemu C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_SKIP_RPATH TRUE)
set(CMAKE_SUPPRESS_REGENERATION TRUE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -O0 -g")

file(GLOB_RECURSE SOURCES
    "src/*.c"
)

include_directories(
    ${CMAKE_SOURCE_DIR}/include/
)

link_libraries(
    m
)

add_executable(${PROJECT_NAME} ${SOURCES})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(compile_run
    COMMAND ${CMAKE_SOURCE_DIR}/run/compile_run.sh
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/run/run.bin
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/run
    VERBATIM
)

add_custom_target(run
    COMMAND ${PROJECT_NAME} run.bin 0x1000
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DEPENDS ${PROJECT_NAME} compile_run
    VERBATIM
)

add_custom_target(debug
    COMMAND ${PROJECT_NAME} run.bin 0x1000 -d
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DEPENDS ${PROJECT_NAME} compile_run
    VERBATIM
)

add_custom_target(tidy
    COMMAND find ./src -name "*.c" -o -name "*.h" | parallel -j 8 clang-format -i {}
    COMMAND find ./include -name "*.c" -o -name "*.h" | parallel -j 8 clang-format -i {}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

add_custom_target(rebuild
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning and rebuilding"
    VERBATIM
)

# Build tests target
add_custom_target(build_tests
    COMMAND ${CMAKE_SOURCE_DIR}/run/build_tests.sh
        ${CMAKE_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
    COMMENT "Building tests (XLEN=32) and converting to binary format"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

# Run tests target
add_custom_target(run_tests
    COMMAND ${CMAKE_SOURCE_DIR}/run/run_tests.sh
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${PROJECT_NAME}
        ${CMAKE_BINARY_DIR}/tests/tmp
        ${CMAKE_SOURCE_DIR}/tests
    COMMENT "Running tests"
    DEPENDS ${PROJECT_NAME} build_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)
