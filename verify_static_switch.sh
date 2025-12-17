#!/bin/bash

# Test script to verify static compile-time log filtering
# This script compiles the test file with different threshold values
# and checks the generated code size to verify filtering works

echo "=== Static Log Switch Verification ==="
echo

# Clean up any previous builds
rm -f test_static_switch_*.o test_static_switch_*.out

# Test with different compile-time thresholds
for threshold in 0 1 2 3; do
    echo "Testing with WW_LOG_COMPILE_THRESHOLD=$threshold"

    # Compile with encode mode and specific threshold
    gcc -c -DWW_LOG_MODE_ENCODE -DWW_LOG_COMPILE_THRESHOLD=$threshold \
        -I./include test_static_switch.c -o test_static_switch_encode_${threshold}.o

    # Get object file size
    size=$(stat -c%s test_static_switch_encode_${threshold}.o 2>/dev/null || stat -f%z test_static_switch_encode_${threshold}.o 2>/dev/null || echo "0")
    echo "  Encode mode object size: $size bytes"

    # Compile with string mode and specific threshold
    gcc -c -DWW_LOG_MODE_STR -DWW_LOG_COMPILE_THRESHOLD=$threshold \
        -I./include test_static_switch.c -o test_static_switch_str_${threshold}.o

    # Get object file size
    size=$(stat -c%s test_static_switch_str_${threshold}.o 2>/dev/null || stat -f%z test_static_switch_str_${threshold}.o 2>/dev/null || echo "0")
    echo "  String mode object size: $size bytes"
    echo
done

echo "=== Expected Behavior ==="
echo "Threshold 0: Only ERR logs should be compiled in"
echo "Threshold 1: ERR and WRN logs should be compiled in"
echo "Threshold 2: ERR, WRN, and INF logs should be compiled in"
echo "Threshold 3: All logs (ERR, WRN, INF, DBG) should be compiled in"
echo
echo "Object file sizes should increase as threshold increases"
echo "if static filtering is working correctly."

# Clean up test files
rm -f test_static_switch_*.o
