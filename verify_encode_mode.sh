#!/bin/bash

echo "========================================="
echo "验证问题2：如何验证代码编译情况？"
echo "========================================="
echo ""

# 编译三种模式
echo "步骤1：编译三种模式"
echo "   编译 str 模式..."
make clean > /dev/null 2>&1
make MODE=str > /dev/null 2>&1
STR_SIZE=$(stat -c%s bin/log_test_str 2>/dev/null || stat -f%z bin/log_test_str 2>/dev/null)
echo "   ✓ str 模式：$(ls -lh bin/log_test_str | awk '{print $5}')"

echo "   编译 encode 模式..."
make clean > /dev/null 2>&1
make MODE=encode > /dev/null 2>&1
ENCODE_SIZE=$(stat -c%s bin/log_test_encode 2>/dev/null || stat -f%z bin/log_test_encode 2>/dev/null)
echo "   ✓ encode 模式：$(ls -lh bin/log_test_encode | awk '{print $5}')"

echo "   编译 disabled 模式..."
make clean > /dev/null 2>&1
make MODE=disabled > /dev/null 2>&1
DISABLED_SIZE=$(stat -c%s bin/log_test_disabled 2>/dev/null || stat -f%z bin/log_test_disabled 2>/dev/null)
echo "   ✓ disabled 模式：$(ls -lh bin/log_test_disabled | awk '{print $5}')"
echo ""

# 方法1：比较二进制大小
echo "========================================="
echo "方法1：比较二进制大小"
echo "========================================="
printf "%-20s %10s\n" "模式" "大小(字节)"
printf "%-20s %10d\n" "str 模式" "$STR_SIZE"
printf "%-20s %10d\n" "encode 模式" "$ENCODE_SIZE"
printf "%-20s %10d\n" "disabled 模式" "$DISABLED_SIZE"
echo ""
printf "str vs encode:    %+d 字节 (%.1f%%)\n" \
    $((STR_SIZE - ENCODE_SIZE)) \
    $(echo "scale=1; ($STR_SIZE - $ENCODE_SIZE) * 100 / $STR_SIZE" | bc)
printf "encode vs disabled: %+d 字节 (%.1f%%)\n" \
    $((ENCODE_SIZE - DISABLED_SIZE)) \
    $(echo "scale=1; ($ENCODE_SIZE - $DISABLED_SIZE) * 100 / $ENCODE_SIZE" | bc)
echo ""

# 方法2：检查字符串
echo "========================================="
echo "方法2：检查 LOG 字符串是否在二进制中"
echo "========================================="
TEST_STRINGS=(
    "Demo module initializing"
    "Processing task"
    "Hardware check"
    "Boot sequence"
    "Application started"
)

echo "检查 str 模式："
for str in "${TEST_STRINGS[@]}"; do
    if strings bin/log_test_str | grep -q "$str"; then
        echo "   ✓ 找到: '$str'"
    else
        echo "   ✗ 未找到: '$str'"
    fi
done
echo ""

echo "检查 encode 模式（应该没有 LOG 字符串）："
FOUND_COUNT=0
for str in "${TEST_STRINGS[@]}"; do
    if strings bin/log_test_encode | grep -q "$str"; then
        echo "   ✗ 意外找到: '$str' (不应该存在！)"
        FOUND_COUNT=$((FOUND_COUNT + 1))
    else
        echo "   ✓ 未找到: '$str' (正确！)"
    fi
done
echo ""

if [ $FOUND_COUNT -eq 0 ]; then
    echo "✅ encode 模式验证通过：没有 LOG 格式字符串被编译进去"
else
    echo "❌ encode 模式验证失败：发现 $FOUND_COUNT 个不应该存在的字符串"
fi
echo ""

# 方法3：检查函数符号
echo "========================================="
echo "方法3：检查函数符号（nm 命令）"
echo "========================================="
echo "str 模式的日志函数："
nm bin/log_test_str | grep "ww_log_str_output" || echo "   (未找到 ww_log_str_output)"
echo ""

echo "encode 模式的日志函数："
nm bin/log_test_encode | grep "ww_log_encode_output" || echo "   (未找到 ww_log_encode_output)"
echo ""

echo "encode 模式中不应该有 str 函数："
if nm bin/log_test_encode | grep -q "ww_log_str_output"; then
    echo "   ❌ 意外找到 ww_log_str_output (不应该存在！)"
else
    echo "   ✓ 未找到 ww_log_str_output (正确！)"
fi
echo ""

# 方法4：生成汇编代码对比
echo "========================================="
echo "方法4：生成预处理代码（宏展开）"
echo "========================================="
echo "生成 demo_init.c 的预处理代码..."

# str 模式预处理
gcc -E src/demo/demo_init.c -I./include -DWW_LOG_MODE_STR -DWW_LOG_LEVEL_THRESHOLD=3 \
    -o /tmp/demo_init_str.i 2>/dev/null

# encode 模式预处理
gcc -E src/demo/demo_init.c -I./include -DWW_LOG_MODE_ENCODE -DWW_LOG_LEVEL_THRESHOLD=3 \
    -o /tmp/demo_init_encode.i 2>/dev/null

echo "   生成文件:"
echo "   - /tmp/demo_init_str.i (str 模式宏展开)"
echo "   - /tmp/demo_init_encode.i (encode 模式宏展开)"
echo ""

echo "查看 LOG_INF 宏展开结果（第一次调用）："
echo ""
echo "--- str 模式 ---"
grep -A 2 '"Demo module initializing"' /tmp/demo_init_str.i | head -5
echo ""
echo "--- encode 模式 ---"
grep -B 2 -A 2 'CURRENT_LOG_ID' /tmp/demo_init_encode.i | grep -A 5 'do {' | head -8
echo ""

echo "========================================="
echo "验证完成！"
echo "========================================="
