# 静态模块开关使用指南

## 概述

日志系统现在支持**双层过滤机制**：

```
┌─────────────────────────────────────────────────────────┐
│              LOG 调用                                    │
│  LOG_INF(WW_LOG_MODULE_DEMO, "Message")                │
└────────────────┬────────────────────────────────────────┘
                 ↓
        ┌────────────────────┐
        │  1. 静态开关检查    │  ← 编译期（WW_LOG_STATIC_CHECK）
        │  (编译期常量)       │
        └────────┬───────────┘
                 ↓
          IF = 0 ?  → 整个代码块被优化掉（零开销）
                 ↓ IF = 1
        ┌────────────────────┐
        │  2. 动态开关检查    │  ← 运行期（g_ww_log_module_mask）
        │  (运行期变量)       │
        └────────┬───────────┘
                 ↓
          IF = 0 ?  → 运行时跳过（微小开销）
                 ↓ IF = 1
        ┌────────────────────┐
        │  3. 级别阈值检查    │  ← 编译期（WW_LOG_LEVEL_THRESHOLD）
        └────────┬───────────┘
                 ↓
        ┌────────────────────┐
        │   执行日志输出      │
        └────────────────────┘
```

---

## 1. 静态开关 vs 动态开关

| 特性 | 静态开关（编译期） | 动态开关（运行期） |
|------|-------------------|-------------------|
| **控制方式** | 编译时 `-D` 宏定义 | 运行时 API 调用 |
| **代码编译** | 禁用模块不编译进去 | 所有模块都编译进去 |
| **二进制大小** | 减小（移除死代码） | 不变 |
| **灵活性** | 固定，需要重新编译 | 动态，无需重新编译 |
| **开销** | 零开销 | 微小 if 判断开销 |
| **适用场景** | 生产环境永久禁用某些模块 | 开发/调试时动态控制 |

---

## 2. 静态开关配置

### 方法1：通过 Makefile（推荐）

```bash
# 禁用单个模块
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"

# 禁用多个模块
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0"

# 禁用除 DEFAULT 外的所有调试模块（生产环境推荐）
make MODE=encode STATIC_OPTS="\
  -DWW_LOG_STATIC_MODULE_DEMO_EN=0 \
  -DWW_LOG_STATIC_MODULE_TEST_EN=0 \
  -DWW_LOG_STATIC_MODULE_DRIVERS_EN=1 \
  -DWW_LOG_STATIC_MODULE_APP_EN=1"
```

### 方法2：通过编译器选项

```bash
gcc -DWW_LOG_STATIC_MODULE_DEMO_EN=0 \
    -DWW_LOG_STATIC_MODULE_TEST_EN=0 \
    -I./include \
    -DWW_LOG_MODE_STR \
    src/demo/demo_init.c -o demo_init.o
```

### 方法3：在 `ww_log_config.h` 中统一配置

```c
// ww_log_config.h 或构建配置文件

// 生产环境：只启用关键模块
#define WW_LOG_STATIC_MODULE_DEFAULT_EN   1  // 必须
#define WW_LOG_STATIC_MODULE_APP_EN       1  // 应用
#define WW_LOG_STATIC_MODULE_DRIVERS_EN   1  // 驱动

// 调试模块：生产环境禁用
#define WW_LOG_STATIC_MODULE_DEMO_EN      0  // 演示代码
#define WW_LOG_STATIC_MODULE_TEST_EN      0  // 测试代码
#define WW_LOG_STATIC_MODULE_BROM_EN      0  // BROM调试
```

---

## 3. 可配置的模块

当前支持的静态开关（可在 `ww_log_modules.h` 中查看）：

| 模块 ID | 模块名 | 静态开关宏 | 默认值 |
|--------|--------|-----------|--------|
| 0 | DEFAULT | `WW_LOG_STATIC_MODULE_DEFAULT_EN` | 1（启用） |
| 1 | DEMO | `WW_LOG_STATIC_MODULE_DEMO_EN` | 1（启用） |
| 2 | TEST | `WW_LOG_STATIC_MODULE_TEST_EN` | 1（启用） |
| 3 | APP | `WW_LOG_STATIC_MODULE_APP_EN` | 1（启用） |
| 4 | DRIVERS | `WW_LOG_STATIC_MODULE_DRIVERS_EN` | 1（启用） |
| 5 | BROM | `WW_LOG_STATIC_MODULE_BROM_EN` | 1（启用） |

**注意：** 新增模块时需要在 `ww_log_modules.h` 中添加对应的静态开关定义和映射。

---

## 4. 验证效果

### 使用验证脚本

```bash
./verify_static_switch.sh
```

**输出示例：**
```
=========================================
静态开关功能验证
=========================================

步骤1：编译基准版本（所有模块静态启用）
   基准大小: 61K (62032 字节)
   ✓ 找到: 'Demo module initializing'

步骤2：静态禁用 DEMO 模块
   禁用 DEMO 后: 60K (60728 字节)
   ✓ 未找到: 'Demo module initializing' (正确优化 ✓)
   💾 节省空间: 1304 字节 (2.10%)

步骤3：静态禁用多个模块（DEMO + TEST）
   禁用后大小: 58K (58664 字节)
   💾 节省空间: 3368 字节 (5.42%)

✅ 静态开关功能正常工作
```

### 手动验证

#### 1. 检查字符串是否被优化掉

```bash
# 编译禁用 DEMO 的版本
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"

# 检查 DEMO 模块字符串（不应该找到）
strings bin/log_test_str | grep "Demo module"
# (无输出) ← 正确！
```

#### 2. 对比二进制大小

```bash
# 基准版本
make MODE=str
ls -lh bin/log_test_str
# 61K

# 禁用 DEMO+TEST
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0 -DWW_LOG_STATIC_MODULE_TEST_EN=0"
ls -lh bin/log_test_str
# 58K (-3K)
```

#### 3. 查看预处理代码

```bash
# 生成预处理文件
gcc -E src/demo/demo_init.c \
    -I./include \
    -DWW_LOG_MODE_STR \
    -DWW_LOG_STATIC_MODULE_DEMO_EN=0 \
    -o demo_init_disabled.i

# 查看 LOG 宏展开
grep -A 5 "Demo module" demo_init_disabled.i
# 应该看到 if (0 && ...) { ... } ← 编译器会优化掉
```

---

## 5. 实际应用场景

### 场景1：开发阶段（全启用）

**配置：** 所有模块静态启用（默认）

```bash
make MODE=str
```

**特点：**
- ✅ 所有 LOG 代码编译进去
- ✅ 运行时可动态控制任意模块
- ✅ 灵活调试

---

### 场景2：功能测试（选择性禁用）

**配置：** 禁用演示模块，保留测试和应用

```bash
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"
```

**特点：**
- ✅ 减小二进制大小
- ✅ 保留必要的测试日志
- ✅ 移除演示代码的日志开销

---

### 场景3：生产环境（最小化）

**配置：** 只启用关键模块

```bash
make MODE=encode STATIC_OPTS="\
  -DWW_LOG_STATIC_MODULE_DEFAULT_EN=1 \
  -DWW_LOG_STATIC_MODULE_DEMO_EN=0 \
  -DWW_LOG_STATIC_MODULE_TEST_EN=0 \
  -DWW_LOG_STATIC_MODULE_APP_EN=1 \
  -DWW_LOG_STATIC_MODULE_DRIVERS_EN=0 \
  -DWW_LOG_STATIC_MODULE_BROM_EN=0"
```

**特点：**
- ✅ 最小二进制大小
- ✅ 只保留应用和系统日志
- ✅ 移除所有调试日志开销
- ✅ 适合资源受限的嵌入式设备

---

### 场景4：调试特定模块

**配置：** 只启用需要调试的模块

```bash
# 只调试 DRIVERS 模块
make MODE=str STATIC_OPTS="\
  -DWW_LOG_STATIC_MODULE_DEFAULT_EN=0 \
  -DWW_LOG_STATIC_MODULE_DEMO_EN=0 \
  -DWW_LOG_STATIC_MODULE_TEST_EN=0 \
  -DWW_LOG_STATIC_MODULE_APP_EN=0 \
  -DWW_LOG_STATIC_MODULE_DRIVERS_EN=1 \
  -DWW_LOG_STATIC_MODULE_BROM_EN=0"
```

**特点：**
- ✅ 极致精简
- ✅ 只看关心的日志
- ✅ 日志输出干净清晰

---

## 6. 结合动态开关使用

静态和动态开关可以**同时使用**，实现灵活的分层控制：

```c
// 编译时配置
// -DWW_LOG_STATIC_MODULE_DEMO_EN=1 (DEMO 模块静态启用)
// -DWW_LOG_STATIC_MODULE_TEST_EN=0 (TEST 模块静态禁用)

int main() {
    ww_log_init();

    // 运行时动态控制（仅对静态启用的模块有效）
    ww_log_disable_module(WW_LOG_MODULE_DEMO);  // ✓ 有效（DEMO 静态启用）
    // ... DEMO 模块日志被禁用

    ww_log_enable_module(WW_LOG_MODULE_TEST);   // ✗ 无效（TEST 静态禁用，代码已不存在）
    // ... TEST 模块日志仍然不会输出

    ww_log_enable_module(WW_LOG_MODULE_DEMO);   // ✓ 有效（重新启用 DEMO）
    // ... DEMO 模块日志恢复输出
}
```

**关键理解：**
- **静态开关** = 编译期决定代码是否存在
- **动态开关** = 运行期决定代码是否执行
- **组合策略** = 静态控制范围，动态控制行为

---

## 7. 优化建议

### 嵌入式设备（Flash/RAM 受限）

```bash
# 策略：使用 encode 模式 + 静态禁用所有调试模块
make MODE=encode STATIC_OPTS="\
  -DWW_LOG_STATIC_MODULE_DEMO_EN=0 \
  -DWW_LOG_STATIC_MODULE_TEST_EN=0"
```

**效果：**
- ✅ encode 模式无格式字符串（节省 Flash）
- ✅ 静态禁用调试模块（进一步节省 Flash）
- ✅ 只保留核心应用日志

---

### 桌面应用（资源充足）

```bash
# 策略：使用 str 模式 + 全静态启用 + 动态控制
make MODE=str
```

**效果：**
- ✅ 人类可读的日志输出
- ✅ 运行时灵活控制
- ✅ 不担心代码大小

---

### 混合环境（开发 → 生产）

**开发阶段：**
```bash
make MODE=str  # 所有模块，方便调试
```

**测试阶段：**
```bash
make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"  # 移除演示代码
```

**生产阶段：**
```bash
make MODE=encode STATIC_OPTS="\
  -DWW_LOG_STATIC_MODULE_DEMO_EN=0 \
  -DWW_LOG_STATIC_MODULE_TEST_EN=0"  # 最小化
```

---

## 8. 常见问题

### Q1: 静态禁用后，动态启用 API 还能用吗？

**A:** 对已静态禁用的模块，动态 API **无效**。原因：代码已经不存在了。

```c
// 编译时：-DWW_LOG_STATIC_MODULE_DEMO_EN=0
ww_log_enable_module(WW_LOG_MODULE_DEMO);  // 调用成功，但无任何效果
LOG_INF(WW_LOG_MODULE_DEMO, "Test");        // 这行代码已被优化掉，不会执行
```

---

### Q2: 如何知道哪些模块被静态禁用了？

**A:** 三种方法：

1. **查看编译选项：**
   ```bash
   make MODE=str STATIC_OPTS="-DWW_LOG_STATIC_MODULE_DEMO_EN=0"
   ```

2. **检查二进制中的字符串：**
   ```bash
   strings bin/log_test_str | grep "Demo module"
   # 无输出 → DEMO 被禁用
   ```

3. **运行验证脚本：**
   ```bash
   ./verify_static_switch.sh
   ```

---

### Q3: 添加新模块后如何支持静态开关？

**A:** 在 `ww_log_modules.h` 中添加三处：

```c
// 1. 添加模块 ID 定义
#define WW_LOG_MODULE_NEW  6  // 新模块

// 2. 添加静态开关定义
#ifndef WW_LOG_STATIC_MODULE_NEW_EN
#define WW_LOG_STATIC_MODULE_NEW_EN  1
#endif

// 3. 添加 ID 映射
#define WW_LOG_STATIC_ENABLED_6  WW_LOG_STATIC_MODULE_NEW_EN
```

完成！新模块自动支持静态开关。

---

### Q4: 为什么 encode 模式静态禁用后大小没变？

**A:** encode 模式本身已经没有格式字符串，所以静态优化的空间有限。但仍然会优化掉：
- 编码逻辑代码
- 参数打包代码
- 函数调用开销

在实际嵌入式项目中，这些优化仍然有价值。

---

## 9. 技术实现细节

### 宏展开过程

```c
// 源代码
LOG_INF(WW_LOG_MODULE_DEMO, "Demo message");

// 第1层展开
_LOG_ENCODE_DISPATCH(WW_LOG_LEVEL_INF, CURRENT_LOG_PARAM, "Demo message")

// 第2层展开（参数计数）
_LOG_ENCODE_0(WW_LOG_LEVEL_INF, "[DEMO]", "Demo message")

// 第3层展开（最终代码）
do {
    U8 _module_id = ((33) >> 5);  // = 1 (WW_LOG_MODULE_DEMO)
    if (WW_LOG_STATIC_CHECK(1) &&        // ← 静态检查（编译期常量）
        WW_LOG_MODULE_ENABLED(1) &&      // ← 动态检查（运行期变量）
        (3 >= 2)) {                      // ← 级别检查（编译期常量）
        ww_log_encode_output(...);
    }
} while(0)

// 当 WW_LOG_STATIC_MODULE_DEMO_EN=0 时
if (0 && WW_LOG_MODULE_ENABLED(1) && (3 >= 2)) {  // ← 编译器识别死代码
    ww_log_encode_output(...);  // ← 整个块被优化掉
}
```

### 编译器优化过程

```
源代码 → 预处理（宏展开） → 编译（死代码消除） → 汇编 → 链接 → 二进制
            ↑                      ↑
       静态开关展开为常量      编译器识别 if (0) 并移除整个块
```

**关键：** 现代编译器（GCC/Clang）在 `-O2` 优化级别下会自动进行死代码消除（Dead Code Elimination）。

---

## 10. 总结

### 静态开关的价值

| 方面 | 价值 |
|------|------|
| **代码大小** | 减少 2%-10% 或更多（取决于禁用的模块数量） |
| **运行性能** | 零开销（代码不存在） |
| **灵活性** | 开发期全启用，生产期选择性禁用 |
| **可维护性** | 通过编译选项控制，无需修改代码 |

### 最佳实践

1. ✅ **开发阶段**：所有模块静态启用（默认）
2. ✅ **测试阶段**：禁用 DEMO 等非必要模块
3. ✅ **生产阶段**：只保留核心模块，结合 encode 模式
4. ✅ **调试问题**：临时启用特定模块重新编译
5. ✅ **CI/CD**：在构建脚本中配置不同环境的静态开关

---

**相关文档：**
- `VERIFICATION_GUIDE.md` - 编译验证指南
- `doc/使用手册.md` - 完整使用手册
- `ww_log_modules.h` - 静态开关定义

**验证工具：**
- `verify_static_switch.sh` - 静态开关验证脚本
- `verify_encode_mode.sh` - 编译效果验证脚本
