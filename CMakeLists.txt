# SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.10)
project(RISCVemu C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_SKIP_RPATH TRUE)
set(CMAKE_SUPPRESS_REGENERATION TRUE)

option(CONFIG_ENABLE_DEBUGGER "Enable built-in debugger (-d flag)" ON)
option(CONFIG_SUPPORT_MISALIGN "Support misaligned memory access (slower)" ON)

option(CONFIG_ENABLE_M_EXTENSION "M Extension: Integer Multiplication/Division" ON)
option(CONFIG_ENABLE_A_EXTENSION "A Extension: Atomic Operations (LR/SC/AMO)" ON)
option(CONFIG_ENABLE_C_EXTENSION "C Extension: 16-bit Compressed Instructions" OFF)
option(CONFIG_ENABLE_F_EXTENSION "F Extension: Single-Precision FPU (requires Zicsr)" ON)
option(CONFIG_ENABLE_D_EXTENSION "D Extension: Double-Precision FPU (requires F)" ON)

option(CONFIG_ENABLE_ZICSR_EXTENSION "Zicsr: CSR Instructions (required for F/D)" ON)
option(CONFIG_ENABLE_ZIFENCEI_EXTENSION "Zifencei: FENCE.I instruction" ON)

option(CONFIG_ENABLE_UART_DEVICE "UART Device (16550-compatible at 0x10000000)" ON)

option(OPTIMIZE_FOR_SIZE "Optimize for binary size (-Os instead of -O3)" OFF)
option(ENABLE_LTO "Enable Link Time Optimization (faster, longer build)" OFF)

mark_as_advanced(
    CMAKE_C_FLAGS
    CMAKE_EXPORT_COMPILE_COMMANDS
    CMAKE_SKIP_RPATH
    CMAKE_SUPPRESS_REGENERATION
)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

if(OPTIMIZE_FOR_SIZE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g")

if(ENABLE_LTO)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()

configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.in
    ${CMAKE_BINARY_DIR}/generated/config.h
    @ONLY
)

include_directories(
    ${CMAKE_SOURCE_DIR}/include/
    ${CMAKE_BINARY_DIR}/generated/
)

file(GLOB_RECURSE SOURCES
    "src/*.c"
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

add_custom_target(build_tests
    COMMAND ${CMAKE_SOURCE_DIR}/run/build_tests.sh
        ${CMAKE_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}
    COMMENT "Building tests (XLEN=32) and converting to binary format"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    VERBATIM
)

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

message(STATUS "")
message(STATUS "RISCVemu Configuration:")
message(STATUS "  Debugger:        ${CONFIG_ENABLE_DEBUGGER}")
message(STATUS "  Misaligned:      ${CONFIG_SUPPORT_MISALIGN}")
message(STATUS "  M Extension:     ${CONFIG_ENABLE_M_EXTENSION}")
message(STATUS "  A Extension:     ${CONFIG_ENABLE_A_EXTENSION}")
message(STATUS "  C Extension:     ${CONFIG_ENABLE_C_EXTENSION}")
message(STATUS "  F Extension:     ${CONFIG_ENABLE_F_EXTENSION}")
message(STATUS "  D Extension:     ${CONFIG_ENABLE_D_EXTENSION}")
message(STATUS "  Zicsr:           ${CONFIG_ENABLE_ZICSR_EXTENSION}")
message(STATUS "  Zifencei:        ${CONFIG_ENABLE_ZIFENCEI_EXTENSION}")
message(STATUS "  UART:            ${CONFIG_ENABLE_UART_DEVICE}")
message(STATUS "  Optimize Size:   ${OPTIMIZE_FOR_SIZE}")
message(STATUS "  LTO:             ${ENABLE_LTO}")
message(STATUS "")
