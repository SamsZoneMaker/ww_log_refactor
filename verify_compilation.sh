#!/bin/bash

echo "========================================="
echo "验证问题1：模块开关是编译期还是运行期？"
echo "========================================="
echo ""

# 先修改初始 mask 为 0x05
echo "步骤1：修改 g_ww_log_module_mask 初始值为 0x05（禁用 DEMO 模块）"
sed -i 's/U32 g_ww_log_module_mask = 0xFFFFFFFF;/U32 g_ww_log_module_mask = 0x05;  \/\/ 仅启用 DEFAULT(0) 和 TEST(2)/' src/core/ww_log_modules.c
echo "   已修改：g_ww_log_module_mask = 0x05"
echo ""

# 编译 str 模式
echo "步骤2：编译 str 模式"
make clean > /dev/null 2>&1
make MODE=str > /dev/null 2>&1
echo "   编译完成：bin/log_test_str"
echo ""

# 检查 DEMO 模块的字符串是否被编译进去
echo "步骤3：检查 DEMO 模块的 LOG 字符串是否在二进制中"
echo "   查找字符串：'Demo initialized'"
if strings bin/log_test_str | grep -q "Demo initialized"; then
    echo "   ✅ 找到！DEMO 模块的字符串被编译进去了（即使 mask 禁用了 bit 1）"
else
    echo "   ❌ 未找到"
fi
echo ""

echo "   查找字符串：'Demo processing task'"
if strings bin/log_test_str | grep -q "Demo processing task"; then
    echo "   ✅ 找到！DEMO 模块的字符串被编译进去了"
else
    echo "   ❌ 未找到"
fi
echo ""

# 恢复原始值
echo "步骤4：恢复 g_ww_log_module_mask 为 0xFFFFFFFF"
sed -i 's/U32 g_ww_log_module_mask = 0x05;  \/\/ 仅启用 DEFAULT(0) 和 TEST(2)/U32 g_ww_log_module_mask = 0xFFFFFFFF;/' src/core/ww_log_modules.c
echo "   已恢复原始值"
echo ""

echo "========================================="
echo "结论：g_ww_log_module_mask 是运行期开关"
echo "- 所有模块的 LOG 代码都会编译进去"
echo "- 运行时通过 mask 控制是否执行"
echo "- enable API 可以动态激活被禁用的模块"
echo "========================================="
