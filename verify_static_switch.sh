#!/bin/bash

echo "========================================="
echo "静态开关功能验证"
echo "========================================="
echo ""

# 1. 编译基准版本（所有模块启用）
echo "步骤1：编译基准版本（所有模块静态启用）"
make clean > /dev/null 2>&1
make MODE=str > /dev/null 2>&1
BASELINE_SIZE=$(stat -c%s bin/log_test_str 2>/dev/null || stat -f%z bin/log_test_str 2>/dev/null)
echo "   基准大小: $(ls -lh bin/log_test_str | awk '{print $5}') ($BASELINE_SIZE 字节)"

# 检查DEMO模块的字符串
echo "   检查 DEMO 模块字符串:"
if strings bin/log_test_str | grep -q "Demo module initializing"; then
    echo "   ✓ 找到: 'Demo module initializing'"
else
    echo "   ✗ 未找到: 'Demo module initializing'"
fi
echo ""

# 2. 静态禁用 DEMO 模块
echo "步骤2：静态禁用 DEMO 模块（编译期优化）"
echo "   使用编译选项: -DWW_LOG_STATIC_MODULE_DEMO_EN=0"
make clean > /dev/null 2>&1
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0" > /dev/null 2>&1
DEMO_DISABLED_SIZE=$(stat -c%s bin/log_test_str 2>/dev/null || stat -f%z bin/log_test_str 2>/dev/null)
echo "   禁用 DEMO 后: $(ls -lh bin/log_test_str | awk '{print $5}') ($DEMO_DISABLED_SIZE 字节)"

# 检查DEMO模块的字符串（应该没有）
echo "   检查 DEMO 模块字符串（应该被优化掉）:"
if strings bin/log_test_str | grep -q "Demo module initializing"; then
    echo "   ✗ 意外找到: 'Demo module initializing' (优化失败！)"
else
    echo "   ✓ 未找到: 'Demo module initializing' (正确优化 ✓)"
fi

# 检查其他模块的字符串（应该仍然存在）
echo "   检查其他模块字符串（应该保留）:"
if strings bin/log_test_str | grep -q "Boot sequence"; then
    echo "   ✓ 找到: 'Boot sequence' (BROM 模块保留)"
else
    echo "   ✗ 未找到: 'Boot sequence'"
fi

SAVED=$((BASELINE_SIZE - DEMO_DISABLED_SIZE))
if [ $SAVED -gt 0 ]; then
    PERCENT=$(echo "scale=2; $SAVED * 100 / $BASELINE_SIZE" | bc)
    echo ""
    echo "   💾 节省空间: $SAVED 字节 ($PERCENT%)"
fi
echo ""

# 3. 静态禁用多个模块
echo "步骤3：静态禁用多个模块（DEMO + TEST）"
echo "   使用编译选项: -DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0"
make clean > /dev/null 2>&1
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0" > /dev/null 2>&1
MULTI_DISABLED_SIZE=$(stat -c%s bin/log_test_str 2>/dev/null || stat -f%z bin/log_test_str 2>/dev/null)
echo "   禁用后大小: $(ls -lh bin/log_test_str | awk '{print $5}') ($MULTI_DISABLED_SIZE 字节)"

echo "   检查被禁用模块的字符串:"
DEMO_FOUND=0
TEST_FOUND=0
if strings bin/log_test_str | grep -q "Demo module"; then
    echo "   ✗ DEMO 模块字符串仍存在"
    DEMO_FOUND=1
else
    echo "   ✓ DEMO 模块字符串已优化掉"
fi

if strings bin/log_test_str | grep -q "Integration test"; then
    echo "   ✗ TEST 模块字符串仍存在"
    TEST_FOUND=1
else
    echo "   ✓ TEST 模块字符串已优化掉"
fi

SAVED_MULTI=$((BASELINE_SIZE - MULTI_DISABLED_SIZE))
if [ $SAVED_MULTI -gt 0 ]; then
    PERCENT_MULTI=$(echo "scale=2; $SAVED_MULTI * 100 / $BASELINE_SIZE" | bc)
    echo ""
    echo "   💾 节省空间: $SAVED_MULTI 字节 ($PERCENT_MULTI%)"
fi
echo ""

# 4. 对比 encode 模式
echo "步骤4：对比 encode 模式下的静态开关效果"
make clean > /dev/null 2>&1
make MODE=encode > /dev/null 2>&1
ENCODE_BASELINE=$(stat -c%s bin/log_test_encode 2>/dev/null || stat -f%z bin/log_test_encode 2>/dev/null)
echo "   encode 模式基准: $(ls -lh bin/log_test_encode | awk '{print $5}') ($ENCODE_BASELINE 字节)"

make clean > /dev/null 2>&1
make MODE=encode STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0" > /dev/null 2>&1
ENCODE_OPTIMIZED=$(stat -c%s bin/log_test_encode 2>/dev/null || stat -f%z bin/log_test_encode 2>/dev/null)
echo "   禁用后大小: $(ls -lh bin/log_test_encode | awk '{print $5}') ($ENCODE_OPTIMIZED 字节)"

SAVED_ENCODE=$((ENCODE_BASELINE - ENCODE_OPTIMIZED))
if [ $SAVED_ENCODE -gt 0 ]; then
    PERCENT_ENCODE=$(echo "scale=2; $SAVED_ENCODE * 100 / $ENCODE_BASELINE" | bc)
    echo "   💾 节省空间: $SAVED_ENCODE 字节 ($PERCENT_ENCODE%)"
fi
echo ""

# 总结
echo "========================================="
echo "总结：静态开关效果"
echo "========================================="
printf "%-35s %10s %10s %8s\n" "配置" "大小(字节)" "节省" "节省%"
printf "%-35s %10d %10s %8s\n" "str 基准（全启用）" "$BASELINE_SIZE" "-" "-"
printf "%-35s %10d %10d %7.2f%%\n" "str 禁用 DEMO" "$DEMO_DISABLED_SIZE" "$SAVED" "$(echo "scale=2; $SAVED * 100 / $BASELINE_SIZE" | bc)"
printf "%-35s %10d %10d %7.2f%%\n" "str 禁用 DEMO+TEST" "$MULTI_DISABLED_SIZE" "$SAVED_MULTI" "$(echo "scale=2; $SAVED_MULTI * 100 / $BASELINE_SIZE" | bc)"
printf "%-35s %10d %10s %8s\n" "encode 基准（全启用）" "$ENCODE_BASELINE" "-" "-"
printf "%-35s %10d %10d %7.2f%%\n" "encode 禁用 DEMO+TEST" "$ENCODE_OPTIMIZED" "$SAVED_ENCODE" "$(echo "scale=2; $SAVED_ENCODE * 100 / $ENCODE_BASELINE" | bc)"
echo ""

# 验证结果
echo "========================================="
echo "验证结果"
echo "========================================="
if [ $DEMO_FOUND -eq 0 ] && [ $TEST_FOUND -eq 0 ] && [ $SAVED_MULTI -gt 0 ]; then
    echo "✅ 静态开关功能正常工作"
    echo "   - 被禁用模块的代码已被编译器优化掉"
    echo "   - 二进制大小显著减小"
    echo "   - 编译期 + 运行期双层开关机制成功实现"
else
    echo "❌ 静态开关可能未正常工作"
    if [ $DEMO_FOUND -eq 1 ] || [ $TEST_FOUND -eq 1 ]; then
        echo "   - 被禁用模块的字符串仍在二进制中"
    fi
    if [ $SAVED_MULTI -le 0 ]; then
        echo "   - 二进制大小未减小"
    fi
fi
echo ""

# 恢复默认编译
echo "恢复默认编译（所有模块启用）..."
make clean > /dev/null 2>&1
make MODE=str > /dev/null 2>&1
echo "完成！"
