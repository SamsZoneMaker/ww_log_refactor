#!/bin/bash
#
# compile_with_auto_ids.sh - Wrapper script for compilation with auto file ID generation
#
# This script automatically generates file IDs before compilation, eliminating
# the need to manually maintain log_file_id.h
#
# Usage:
#   ./scripts/compile_with_auto_ids.sh [options]
#
# Options:
#   --force      Force file ID regeneration (ignore cache)
#   --verbose    Show detailed file scanning information
#   --help       Show this help message
#
# Environment Variables:
#   SKIP_FILE_ID_GEN    Set to '1' to skip file ID generation (for debugging)
#

set -e  # Exit on error

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse command line arguments
FORCE_FLAG=""
VERBOSE_FLAG=""

while [[ $# -gt 0 ]]; do
    case $1 in
        --force)
            FORCE_FLAG="--force"
            shift
            ;;
        --verbose)
            VERBOSE_FLAG="--verbose"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --force      Force file ID regeneration (ignore cache)"
            echo "  --verbose    Show detailed file scanning information"
            echo "  --help       Show this help message"
            echo ""
            echo "Environment Variables:"
            echo "  SKIP_FILE_ID_GEN    Set to '1' to skip file ID generation"
            exit 0
            ;;
        *)
            echo -e "${RED}Error: Unknown option: $1${NC}"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Project root directory (parent of scripts/)
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Build with Auto File ID Generation${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Step 1: Auto-generate file IDs (unless skipped)
if [[ "$SKIP_FILE_ID_GEN" == "1" ]]; then
    echo -e "${YELLOW}⏭️  Skipping file ID generation (SKIP_FILE_ID_GEN=1)${NC}"
else
    echo -e "${GREEN}Step 1: Auto-generating file IDs...${NC}"

    # Check if Python 3 is available
    if ! command -v python3 &> /dev/null; then
        echo -e "${RED}❌ Error: python3 not found${NC}"
        echo "Please install Python 3 to use auto file ID generation"
        exit 1
    fi

    # Run the file ID generator
    python3 tools/generate_file_ids.py $FORCE_FLAG $VERBOSE_FLAG

    if [ $? -ne 0 ]; then
        echo -e "${RED}❌ File ID generation failed${NC}"
        exit 1
    fi
    echo ""
fi

# Step 2: Compile the project
echo -e "${GREEN}Step 2: Compiling project...${NC}"

# Check if Makefile exists
if [ -f "Makefile" ]; then
    # Use Makefile for incremental compilation
    echo "Using Makefile for compilation..."
    make clean
    make -j$(nproc 2>/dev/null || echo 4)

    if [ $? -ne 0 ]; then
        echo -e "${RED}❌ Compilation failed${NC}"
        exit 1
    fi
else
    # Fallback: Simple shell script compilation
    echo "Makefile not found, using simple compilation..."

    # Clean previous build
    rm -rf build/ bin/
    mkdir -p build/ bin/

    # Compile all .c files
    echo "Compiling source files..."
    for c_file in $(find src/ -name "*.c" | sort); do
        obj_file="build/$(basename ${c_file%.c}).o"
        echo "  CC  $c_file"
        gcc -c "$c_file" -Iinclude -O2 -Wall -Wextra -o "$obj_file"

        if [ $? -ne 0 ]; then
            echo -e "${RED}❌ Compilation failed: $c_file${NC}"
            exit 1
        fi
    done

    # Link
    echo "Linking..."
    gcc build/*.o -o bin/log_test

    if [ $? -ne 0 ]; then
        echo -e "${RED}❌ Linking failed${NC}"
        exit 1
    fi
fi

echo ""
echo -e "${GREEN}✅ Build complete!${NC}"
echo ""

# Show binary size
if [ -f "bin/log_test" ]; then
    SIZE=$(stat -c%s "bin/log_test" 2>/dev/null || stat -f%z "bin/log_test" 2>/dev/null)
    echo "Binary size: $SIZE bytes"
fi

echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}  Build finished successfully${NC}"
echo -e "${BLUE}========================================${NC}"
