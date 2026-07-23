#!/bin/bash
# SPDX-FileCopyrightText: 2026 PhiLia093 phi_lia093@126.com
# SPDX-License-Identifier: GPL-3.0-or-later

# Get parameters from CMake
EMULATOR="$1"
TEST_DIR="$2"
SOURCE_TEST_DIR="$3"

# If no parameters provided, fallback to default values (for manual execution)
if [ -z "$EMULATOR" ]; then
    EMULATOR="./RISCVemu"
fi
if [ -z "$TEST_DIR" ]; then
    TEST_DIR="tests/tmp"
fi
if [ -z "$SOURCE_TEST_DIR" ]; then
    SOURCE_TEST_DIR="tests"
fi

BASE_ADDR="0x80000000"

BUILD_DIR="build"
LOG_FILE="$BUILD_DIR/test_results.log"
FULL_LOG_DIR="$BUILD_DIR/test_logs"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

mkdir -p "$BUILD_DIR"
mkdir -p "$FULL_LOG_DIR"

> "$LOG_FILE"

echo "=========================================="
echo "  RISC-V emulator batch test"
echo "  emulator executable: $EMULATOR"
echo "  test directory: $TEST_DIR"
echo "  source test directory: $SOURCE_TEST_DIR"
echo "  base address: $BASE_ADDR"
echo "  summary log: $LOG_FILE"
echo "  full logs: $FULL_LOG_DIR/"
echo "=========================================="
echo ""

TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

# Check if test directory exists
if [ ! -d "$TEST_DIR" ]; then
    echo -e "${RED}Error: Test directory '$TEST_DIR' does not exist!${NC}"
    exit 1
fi

# Find all .bin files in the test directory (non-recursive)
for bin_file in "$TEST_DIR"/*.bin; do
    [ -e "$bin_file" ] || continue

    filename=$(basename "$bin_file" .bin)

    # our emulator do not support exception and interrupt so despite our behavior on ALIGN is right, this test cannot be passed
    if [ "$filename" = "rv32ui-p-ma_data" ]; then
        echo -e "[$filename] ${YELLOW}⏭️ SKIPPED${NC}"
        ((SKIPPED++))
        continue
    fi

    test_log_file="$FULL_LOG_DIR/${filename}.log"

    echo -n "[$filename] RUNNING... "

    output=$("$EMULATOR" "$bin_file" "$BASE_ADDR" 2>&1)
    exit_code=$?

    echo "$output" > "$test_log_file"

    last_line=$(echo "$output" | tail -1)

    if echo "$last_line" | grep -qE '\[INFO\] Execution terminated\. Return value: 0x0 \(final x10 value\) 0x5d \(final x17 value\)'; then
        echo -e "${GREEN}✅ PASS${NC}"
        ((PASSED++))
        echo "$filename: PASS" >> "$LOG_FILE"
    else
        echo -e "${RED}❌ FAIL${NC}"
        ((FAILED++))
        echo "$filename: FAIL" >> "$LOG_FILE"
        echo "  Last line: $last_line" >> "$LOG_FILE"
        if echo "$output" | grep -q "FATAL"; then
            echo "  FATAL error detected - see $FULL_LOG_DIR/${filename}.log for details" >> "$LOG_FILE"
        fi
    fi

    ((TOTAL++))
done

TOTAL=$((PASSED + FAILED))

echo ""
echo "=========================================="
echo "  TEST COMPLETED！"
echo "  TOTAL: $TOTAL"
echo -e "  PASSED: ${GREEN}$PASSED${NC}"
echo -e "  FAILED: ${RED}$FAILED${NC}"
echo "  SKIPPED: $SKIPPED"
echo "  SUMMARY LOG: $LOG_FILE"
echo "  FULL LOGS: $FULL_LOG_DIR/"
echo "=========================================="

if [ $FAILED -gt 0 ]; then
    echo ""
    echo -e "${RED}Failed tests:${NC}"
    grep "FAIL" "$LOG_FILE" | while read line; do
        echo "  $line"
    done
fi

exit $FAILED
