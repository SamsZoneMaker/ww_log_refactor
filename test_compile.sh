#!/bin/bash
# Simple compilation test for encode output modes

echo "=========================================="
echo "Testing Encode Output Modes"
echo "=========================================="
echo ""

# Test 1: UART output mode (default)
echo "Test 1: Compiling with UART output (WW_LOG_ENCODE_OUTPUT_TO_RAM=0)..."
gcc -o test_uart \
    -I./include \
    -DWW_LOG_MODE_ENCODE \
    -DWW_LOG_ENCODE_OUTPUT_TO_RAM=0 \
    -DCURRENT_MODULE_ID=0 \
    -DCURRENT_FILE_ID=0 \
    -DCURRENT_MODULE_STATIC_EN=1 \
    test_encode_output.c \
    core/ww_log_encode.c \
    core/ww_log_common.c \
    core/ww_log_modules.c \
    2>&1

if [ $? -eq 0 ]; then
    echo "  [OK] UART mode compiled successfully"
    echo ""
    echo "Running UART mode test:"
    echo "----------------------------------------"
    ./test_uartecho ""
else
    echo "  [FAIL] UART mode compilation failed"
    echo ""
fi

# Test 2: RAM output mode
echo "Test 2: Compiling with RAM output (WW_LOG_ENCODE_OUTPUT_TO_RAM=1)..."
gcc -o test_ram \
    -I./include \
    -DWW_LOG_MODE_ENCODE \
    -DWW_LOG_ENCODE_OUTPUT_TO_RAM=1 \
    -DCURRENT_MODULE_ID=0 \
    -DCURRENT_FILE_ID=0 \
    -DCURRENT_MODULE_STATIC_EN=1 \
    test_encode_output.c \
    core/ww_log_encode.c \
    core/ww_log_common.c \
    core/ww_log_modules.c \
    core/ww_log_ram.c \
    2>&1

if [ $? -eq 0 ]; then
    echo "  [OK] RAM mode compiled successfully"
    echo ""
    echo "Running RAM mode test:"
    echo "----------------------------------------"
    ./test_ram
    echo ""
else
    echo "  [FAIL] RAM mode compilation failed"
    echo ""
fi

echo "=========================================="
echo "Test Complete"
echo "=========================================="
