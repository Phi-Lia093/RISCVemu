cmake_minimum_required(VERSION 3.10)
project(RISCVemu C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

find_package(Curses REQUIRED)

file(GLOB_RECURSE SOURCES 
    "src/*.c"
)

include_directories(
    ${CURSES_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include/
)

link_libraries(
    ${CURSES_LIBRARIES}
    m
)

add_executable(${PROJECT_NAME} ${SOURCES})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_SOURCE_DIR}/test/mktest.sh
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
    COMMENT "Running test/mktest.sh"
    VERBATIM
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/test/test.bin
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/
    COMMENT "Copying test.bin to output directory"
    VERBATIM
)

add_custom_target(run
    COMMAND ./RISCVemu
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