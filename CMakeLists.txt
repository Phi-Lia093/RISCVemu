cmake_minimum_required(VERSION 3.10)
project(RISCVemu C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -O3")


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

add_custom_target(mktest
    COMMAND ${CMAKE_SOURCE_DIR}/test/mktest.sh
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/test/test.bin
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    COMMENT "Running test/mktest.sh"
    VERBATIM
)

add_custom_target(run
    COMMAND ./RISCVemu test.bin 0x1000
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DEPENDS ${PROJECT_NAME}
    VERBATIM
)

add_custom_target(debug
    COMMAND ./RISCVemu test.bin 0x1000 -d
    WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    DEPENDS ${PROJECT_NAME}
    VERBATIM
)

add_custom_target(tidy
    COMMAND find ./src -name "*.c" -o -name "*.h" | parallel -j 8 clang-format -i {}
    COMMAND find ./include -name "*.c" -o -name "*.h" | parallel -j 8 clang-format -i {}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)
