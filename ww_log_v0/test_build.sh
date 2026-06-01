#!/bin/bash
# Build test script for MSYS2/Linux
# This script tests the new log system

set -e  # Exit on error

echo "========================================"
echo "Log System Build Test"
echo "========================================"
echo ""

# Check if we're in the right directory
if [ ! -f "log_config.json" ]; then
    echo "Error: log_config.json not found!"
    echo "Please run this script from the project root directory."
    exit 1
fi

# Step 1: Generate file ID mappings
echo "Step 1: Generating file ID mappings..."
mkdir -p build
mkdir -p include

python3 tools/gen_file_ids.py log_config.json --makefile > build/file_ids.mk
python3 tools/gen_file_ids.py log_config.json --header > include/auto_file_ids.h

echo "  [OK] File ID mappings generated"
echo ""

# Step 2: Show generated files
echo "Step 2: Verifying generated files..."
if [ -f "build/file_ids.mk" ]; then
    echo "  [OK] build/file_ids.mk exists"
    file_count=$(grep -c "FILE_ID_" build/file_ids.mk || true)
    echo "  [INFO] $file_count file IDs configured"
else
    echo "  [FAIL] build/file_ids.mk not found"
    exit 1
fi

if [ -f "include/auto_file_ids.h" ]; then
    echo "  [OK] include/auto_file_ids.h exists"
else
    echo "  [FAIL] include/auto_file_ids.h not found"
    exit 1
fi
echo ""

# Step 3: Show sample of generated mappings
echo "Step 3: Sample file ID mappings..."
head -n 20 build/file_ids.mk
echo ""

# Step 4: Try to compile
echo "Step 4: Attempting compilation..."
echo ""

if command -v make &> /dev/null; then
    echo "Make is available. Starting build..."
    echo ""
    echo "Note: Log mode is configured in include/ww_log.h"
    echo "Current mode setting will be used for compilation."
    echo ""

    # Clean
    echo "Cleaning..."
    make clean
    echo ""

    # Build with current mode
    echo "Building with current mode..."
    make
    echo ""

    echo "========================================"
    echo "Build completed successfully!"
    echo "========================================"
    echo ""
    echo "To change log mode:"
    echo "  1. Edit include/ww_log.h"
    echo "  2. Uncomment desired mode (WW_LOG_MODE_STR, WW_LOG_MODE_ENCODE, or WW_LOG_MODE_DISABLED)"
    echo "  3. Run: make clean && make"
    echo ""echo "You can now run:"
    echo "  make run"
    echo ""
else
    echo "Make command not found in PATH."
    echo "Please ensure make is installed and accessible."
    echo ""
    echo "File ID mappings have been generated successfully."
    echo "You can manually compile or configure your environment."
    exit 1
fi
