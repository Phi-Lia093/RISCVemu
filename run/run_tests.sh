#!/bin/bash

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
LOG_FILE="test_results.log"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

> "$LOG_FILE"

echo "=========================================="
echo "  RISC-V emulator batch test"
echo "  emulator executable: $EMULATOR"
echo "  test directory: $TEST_DIR"
echo "  source test directory: $SOURCE_TEST_DIR"
echo "  base address: $BASE_ADDR"
echo "=========================================="
echo ""

TOTAL=0
PASSED=0
FAILED=0

# Check if test directory exists
if [ ! -d "$TEST_DIR" ]; then
    echo -e "${RED}Error: Test directory '$TEST_DIR' does not exist!${NC}"
    exit 1
fi

# Find all .bin files in the test directory (non-recursive)
for bin_file in "$TEST_DIR"/*.bin; do
    [ -e "$bin_file" ] || continue

    filename=$(basename "$bin_file" .bin)

    echo -n "[$filename] RUNNING... "

    output=$("$EMULATOR" "$bin_file" "$BASE_ADDR" 2>&1)
    exit_code=$?

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
    fi

    ((TOTAL++))
done

echo ""
echo "=========================================="
echo "  TEST COMPLETED！"
echo "  TOTAL: $TOTAL"
echo -e "  PASSED: ${GREEN}$PASSED${NC}"
echo -e "  FAILED: ${RED}$FAILED${NC}"
echo "  LOG: $LOG_FILE"
echo "=========================================="

exit $FAILED
