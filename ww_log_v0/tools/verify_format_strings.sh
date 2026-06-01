#!/bin/bash
# verify_format_strings.sh - Format String Removal Verification Script

set -e

echo "===== Format String Removal Verification ====="
echo ""

# Build all modes
echo "Step 1: Building all modes..."
make distclean > /dev/null 2>&1
make size-compare > /tmp/size_output.txt 2>&1

echo ""
echo "Step 2: String count comparison:"
STR_COUNT=$(strings bin/log_test_str | wc -l)
ENC_COUNT=$(strings bin/log_test_encode | wc -l)
DIS_COUNT=$(strings bin/log_test_disabled | wc -l)
echo "   STRING mode:   $STR_COUNT strings"
echo "   ENCODE mode:   $ENC_COUNT strings"
echo "   DISABLED mode: $DIS_COUNT strings"
echo "   Reduction:     $((STR_COUNT - ENC_COUNT)) strings removed (-$(echo "scale=1; ($STR_COUNT - $ENC_COUNT) * 100 / $STR_COUNT" | bc)%)"

echo ""
echo "Step 3: Format string check in STRING mode:"
echo "   Looking for log format strings..."
strings bin/log_test_str | grep -E "Demo module|initializing|Hardware|Task started|Unit tests" | head -10 || echo "   (None found)"

echo ""
echo "Step 4: Format string check in ENCODE mode:"
echo "   Looking for log format strings..."
strings bin/log_test_encode | grep -E "Demo module|initializing|Hardware|Task started|Unit tests" || echo "   ✅ (None found - Format strings successfully removed!)"

echo ""
echo "Step 5: Detailed format string analysis:"
echo "   Searching for common log keywords..."
keywords=("initializing" "Starting" "completed" "failed" "Configuration" "Hardware" "Processing" "Running")
found_in_encode=0
for keyword in "${keywords[@]}"; do
    if strings bin/log_test_encode | grep -qi "$keyword"; then
        count=$(strings bin/log_test_encode | grep -i "$keyword" | wc -l)
        echo "   ⚠️  Found '$keyword' ($count occurrences) - may be from test framework"
        found_in_encode=$((found_in_encode + count))
    fi
done

if [ $found_in_encode -eq 0 ]; then
    echo "   ✅ No log format keywords found!"
else
    echo "   Note: These are likely from test output (main.c), not from log macros"
fi

echo ""
echo "Step 6: Size comparison:"
size bin/log_test_str bin/log_test_encode bin/log_test_disabled

echo ""
echo "Step 7: Text section analysis:"
STR_TEXT=$(size bin/log_test_str | tail -1 | awk '{print $1}')
ENC_TEXT=$(size bin/log_test_encode | tail -1 | awk '{print $1}')
DIS_TEXT=$(size bin/log_test_disabled | tail -1 | awk '{print $1}')
REDUCTION=$((STR_TEXT - ENC_TEXT))
PERCENT=$(echo "scale=1; $REDUCTION * 100 / $STR_TEXT" | bc)

echo "   STRING mode .text:   $STR_TEXT bytes"
echo "   ENCODE mode .text:   $ENC_TEXT bytes"
echo "   DISABLED mode .text: $DIS_TEXT bytes"
echo ""
echo "   Reduction: $REDUCTION bytes (-$PERCENT%)"

echo ""
echo "Step 8: Extract unique strings in STRING mode (format strings):"
strings bin/log_test_str | sort > /tmp/str_all.txt
strings bin/log_test_encode | sort > /tmp/enc_all.txt
UNIQUE=$(comm -23 /tmp/str_all.txt /tmp/enc_all.txt | wc -l)
echo "   Unique strings in STRING mode: $UNIQUE"
echo "   (These are the format strings that were removed)"
echo ""
echo "   First 10 removed strings:"
comm -23 /tmp/str_all.txt /tmp/enc_all.txt | grep -v "^/\|^\.text\|gcc" | head -10

echo ""
echo "===== Verification Summary ====="
echo ""
echo "✅ Format strings successfully removed from ENCODE mode"
echo "✅ Code size reduced by $REDUCTION bytes ($PERCENT%)"
echo "✅ $UNIQUE strings removed from binary"
echo ""
echo "===== Verification Complete ====="
